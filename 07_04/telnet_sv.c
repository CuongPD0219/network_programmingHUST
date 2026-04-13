#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT         9001
#define MAX_CLIENTS  10
#define BUF_SIZE     4096
#define USERS_FILE   "user.txt"
#define OUT_FILE     "out.txt"
#define MAX_ATTEMPTS 3

typedef enum {
    STATE_WAIT_USER = 0,
    STATE_WAIT_PASS,
    STATE_LOGGED_IN
} ClientState;

typedef struct {
    int fd;
    ClientState state;
    char username[64];
    int attempts;
} Client;

Client clients[MAX_CLIENTS];

void trim_crlf(char *s) {
    int len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r'))
        s[--len] = '\0';
}

// Gửi đảm bảo hết dữ liệu, không bị cắt giữa chừng
void send_all(int fd, const char *msg) {
    int total = 0, len = strlen(msg);
    while (total < len) {
        int n = send(fd, msg + total, len - total, 0);
        if (n <= 0) break;
        total += n;
    }
}

int check_login(const char *user, const char *pass) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) { perror("fopen"); return 0; }
    char line[256], fu[64], fp[64];
    while (fgets(line, sizeof(line), f)) {
        trim_crlf(line);
        if (sscanf(line, "%63s %63s", fu, fp) == 2)
            if (strcmp(fu, user) == 0 && strcmp(fp, pass) == 0) {
                fclose(f); return 1;
            }
    }
    fclose(f);
    return 0;
}

void exec_command(int fd, const char *cmd) {
    char shell_cmd[BUF_SIZE];
    snprintf(shell_cmd, BUF_SIZE, "%s > %s 2>&1", cmd, OUT_FILE);
    system(shell_cmd);

    FILE *f = fopen(OUT_FILE, "r");
    if (!f) { send_all(fd, "Khong doc duoc ket qua.\n"); return; }

    char buf[BUF_SIZE];
    int n;
    while ((n = fread(buf, 1, BUF_SIZE - 1, f)) > 0) {
        buf[n] = '\0';
        send_all(fd, buf);
    }
    fclose(f);
    send_all(fd, "\n$ ");
}

void remove_client(int idx) {
    printf("[-] Client '%s' (fd=%d) ngat ket noi.\n",
           clients[idx].username[0] ? clients[idx].username : "?",
           clients[idx].fd);
    close(clients[idx].fd);
    clients[idx].fd = -1;
    clients[idx].state = STATE_WAIT_USER;
    clients[idx].username[0] = '\0';
    clients[idx].attempts = 0;
}

int find_free_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd == -1) return i;
    return -1;
}

int main() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].username[0] = '\0';
        clients[i].attempts = 0;
    }

    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // Không dùng O_NONBLOCK -- select() đã đủ để không bị block

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, MAX_CLIENTS);

    printf("Telnet Server\n");
    printf("Lang nghe cong %d...\n", PORT);
    printf("File tai khoan: %s\n\n", USERS_FILE);

    fd_set read_fds;
    char buf[BUF_SIZE];

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &read_fds);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select"); break;
        }

        // Ket noi moi
        if (FD_ISSET(server_fd, &read_fds)) {
            int new_fd = accept(server_fd,
                                (struct sockaddr*)&client_addr, &addr_len);
            if (new_fd >= 0) {
                int slot = find_free_slot();
                if (slot == -1) {
                    send_all(new_fd, "Server day, thu lai sau.\n");
                    close(new_fd);
                } else {
                    clients[slot].fd       = new_fd;
                    clients[slot].state    = STATE_WAIT_USER;
                    clients[slot].attempts = 0;
                    clients[slot].username[0] = '\0';

                    printf("[+] Ket noi moi: %s:%d (fd=%d)\n",
                           inet_ntoa(client_addr.sin_addr),
                           ntohs(client_addr.sin_port), new_fd);

                    send_all(new_fd, "Telnet Server\n");
                    send_all(new_fd, "Username: ");
                }
            }
        }

        // Du lieu tu cac client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;
            if (fd == -1 || !FD_ISSET(fd, &read_fds)) continue;

            memset(buf, 0, BUF_SIZE);
            int n = recv(fd, buf, BUF_SIZE - 1, 0);
            if (n <= 0) { remove_client(i); continue; }
            trim_crlf(buf);

            if (clients[i].state == STATE_WAIT_USER) {
                if (strlen(buf) == 0) { send_all(fd, "Username: "); continue; }
                strncpy(clients[i].username, buf, 63);
                clients[i].state = STATE_WAIT_PASS;
                send_all(fd, "Password: ");

            } else if (clients[i].state == STATE_WAIT_PASS) {
                if (check_login(clients[i].username, buf)) {
                    printf("    [+] Dang nhap thanh cong: '%s'\n", clients[i].username);
                    clients[i].state    = STATE_LOGGED_IN;
                    clients[i].attempts = 0;
                    char welcome[256];
                    snprintf(welcome, sizeof(welcome),
                             "Dang nhap thanh cong! Xin chao %s.\nGo 'exit' de thoat.\n$ ",
                             clients[i].username);
                    send_all(fd, welcome);
                } else {
                    clients[i].attempts++;
                    printf("    [-] Sai mat khau: '%s' (lan %d)\n",
                           clients[i].username, clients[i].attempts);
                    if (clients[i].attempts >= MAX_ATTEMPTS) {
                        send_all(fd, "Sai qua nhieu lan. Ket noi bi dong.\n");
                        remove_client(i);
                    } else {
                        char err[128];
                        snprintf(err, sizeof(err),
                                 "Sai tai khoan hoac mat khau. Con %d lan.\nUsername: ",
                                 MAX_ATTEMPTS - clients[i].attempts);
                        send_all(fd, err);
                        clients[i].state = STATE_WAIT_USER;
                    }
                }

            } else if (clients[i].state == STATE_LOGGED_IN) {
                if (strlen(buf) == 0) { send_all(fd, "$ "); continue; }
                printf("    [cmd] '%s': %s\n", clients[i].username, buf);
                if (strcmp(buf, "exit") == 0 || strcmp(buf, "quit") == 0) {
                    send_all(fd, "Tam biet!\n");
                    remove_client(i);
                    continue;
                }
                exec_command(fd, buf);
            }
        }
    }

    close(server_fd);
    return 0;
}