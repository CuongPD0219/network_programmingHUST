#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define PORT        8080
#define MAX_CLIENTS 100
#define BUF_SIZE    256

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server is listening on port %d...\n", PORT);

    struct pollfd fds[MAX_CLIENTS];
    // login[i] tương ứng fds[i]: 0 = chua dang nhap, 1 = da dang nhap
    int login[MAX_CLIENTS];
    int nfds = 1;

    fds[0].fd     = listener;
    fds[0].events = POLLIN;
    memset(login, 0, sizeof(login));

    char buf[BUF_SIZE];

    while (1) {
        int ret = poll(fds, nfds, 60000);
        if (ret < 0) {
            perror("poll() failed");
            break;
        }
        if (ret == 0) {
            printf("Timed out.\n");
            continue;
        }

        // co client moi ket noi
        if (fds[0].revents & POLLIN) {
            int client = accept(listener, NULL, NULL);
            if (client >= 0) {
                if (nfds < MAX_CLIENTS) {
                    printf("New client connected %d\n", client);
                    fds[nfds].fd     = client;
                    fds[nfds].events = POLLIN;
                    login[nfds]      = 0;
                    nfds++;

                    char *msg = "Hay dang nhap: <username> <password>\n";
                    send(client, msg, strlen(msg), 0);
                } else {
                    close(client);
                }
            }
        }

        // xu ly tung client 
        for (int i = 1; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            memset(buf, 0, BUF_SIZE);
            ret = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);

            if (ret <= 0) {
                printf("Client %d disconnected\n", fds[i].fd);
                close(fds[i].fd);
                // xoa client khoi mang khi ngat ket noi
                fds[i]   = fds[nfds - 1];
                login[i] = login[nfds - 1];
                nfds--;
                i--;
                continue;
            }

            buf[ret] = 0;
            // bo ky tu xuong dong 
            if (strlen(buf) > 0 && buf[strlen(buf) - 1] == '\n')
                buf[strlen(buf) - 1] = '\0';

            printf("Received from %d: %s\n", fds[i].fd, buf);

            if (login[i] == 0) {
                // nhan "user pass" tren 1 dong
                char user[32], pass[32], tmp[64];
                int n = sscanf(buf, "%31s %31s %63s", user, pass, tmp);
                if (n != 2) {
                    char *msg = "Sai cu phap. Hay dang nhap lai: <username> <password>\n";
                    send(fds[i].fd, msg, strlen(msg), 0);
                } else {
                    // ghep lai de so sanh voi tung dong trong users.txt
                    char cred[64];
                    sprintf(cred, "%s %s", user, pass);

                    int found = 0;
                    char line[64];
                    FILE *f = fopen("users.txt", "r");
                    if (f) {
                        while (fgets(line, sizeof(line), f) != NULL) {
                            if (strlen(line) > 0 && line[strlen(line) - 1] == '\n')
                                line[strlen(line) - 1] = '\0';
                            if (strcmp(line, cred) == 0) {
                                found = 1;
                                break;
                            }
                        }
                        fclose(f);
                    } else {
                        fprintf(stderr, "Khong mo duoc users.txt\n");
                    }

                    if (found) {
                        char *msg = "OK. Hay nhap lenh.\n";
                        send(fds[i].fd, msg, strlen(msg), 0);
                        login[i] = 1;
                    } else {
                        char *msg = "Sai username hoac password. Hay dang nhap lai.\n";
                        send(fds[i].fd, msg, strlen(msg), 0);
                    }
                }
            } else {
                // da dang nhap: thuc thi lenh, gui ket qua ve
                if (strcmp(buf, "exit") == 0) {
                    send(fds[i].fd, "Bye!\n", 5, 0);
                    close(fds[i].fd);
                    fds[i]   = fds[nfds - 1];
                    login[i] = login[nfds - 1];
                    nfds--;
                    i--;
                    continue;
                }

                
                char cmd[512];
                sprintf(cmd, "%s > out.txt 2>&1", buf);
                system(cmd);

                FILE *f = fopen("out.txt", "rb");
                if (f) {
                    int len;
                    while ((len = fread(buf, 1, sizeof(buf), f)) > 0)
                        send(fds[i].fd, buf, len, 0);
                    fclose(f);
                }
            }
        }
    }

    close(listener);
    return 0;
}