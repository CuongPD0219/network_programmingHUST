#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_CLIENT 10
#define MAX_BUFFER 1024

int check_login( const char *str){
    char user[32], pass[32], tmp[64];
    int n = sscanf(str, "%31s %31s %63s", user, pass, tmp);
    if (n != 2) {
        return 0;
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

        return found;
    }
}

void signal_handler(int sig) {
    int pid = wait(NULL);
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Child process terminated: %d\n", pid);
    }
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    if (listen(listener, MAX_CLIENT)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }
    
    // Server is now listening for incoming connections
    printf("Server is listening on port 8080...\n");
    
    signal(SIGCHLD, signal_handler);
    char buf[256];
             
    
    while (1) {
        int client = accept(listener, NULL, NULL);
        printf("=========New client connected: %d=========\n", client);

        char *msg = "Hay dang nhap: <username> <password>\n";
        send(client, msg, strlen(msg), 0);

        if (fork() == 0) {
            // Xu ly trong tien trinh con
            close(listener);
            int is_authenticated = 0;

            // tao ten file ghi ket qua
            char out_file[32];
            sprintf(out_file, "out_%d.txt", getpid());

            while (1) {
                memset(buf, 0 , 255);
                int len = recv(client, buf, sizeof(buf), 0);
                if (len <= 0){
                    printf("=========Client %d disconnected=========\n", client);
                    break;
                }

                // xu ly du lieu dau vao
                buf[strcspn(buf, "\r\n")] = 0;

                if(!is_authenticated){
                    if(check_login(buf)){
                        is_authenticated = 1;
                        char *msg = "Dang nhap thanh cong! Nhap lenh de thuc thi.\n";
                        send(client, msg, strlen(msg), 0);
                        printf("\t[Process: %d]: dang nhap thanh cong\n", getpid());
                    }else{
                        char *msg = "Dang nhap that bai! Vui long nhap lai tai khoan.\n";
                        send(client, msg, strlen(msg), 0);
                    }
                }else{
                    if(strcmp(buf, "exit") ==0){
                        printf("=========Client %d disconnected=========\n", client);
                        break;
                    } 

                    printf("[Process: %d]: %s\n", getpid(), buf);

                    char cmd[512];
                    sprintf(cmd, "%s > %s 2>&1", buf, out_file);
                    system(cmd);

                    char message[MAX_BUFFER];

                    FILE *f = fopen(out_file, "rb");
                    if (f) {
                        int countWord;
                        while ((countWord = fread(message, 1, sizeof(message), f)) > 0)
                            send(client, message, countWord, 0);
                        memset(message, sizeof(message), 0);
                        fclose(f);
                    }else {
                        char *err_msg = "Loi: Khong the doc ket qua lenh.\n";
                        send(client, err_msg, strlen(err_msg), 0);
                    }
                }
            }
            remove(out_file);
            close(client);
            exit(0);
        }
        // Xu ly o tien trinh cha
        close(client);
        
    }

    close(listener);
    return 0;
}