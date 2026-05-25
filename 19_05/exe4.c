//# telnet server : multithread
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define PORT        8080
#define BUF_SIZE    256

void *client_handler(void *arg);

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
    addr.sin_addr.s_addr = INADDR_ANY;
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

    printf("Telnet Server is listening on port %d... \n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            perror("accept() failed");
            continue;
        }

        printf("New client connected: fd=%d\n", client);

        int *client_sock = malloc(sizeof(int));
        *client_sock = client;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, (void*)client_sock) != 0) {
            perror("pthread_create() failed");
            close(client);
            free(client_sock);
        }

        // Tách luồng để hệ thống tự dọn dẹp tài nguyên khi luồng kết thúc
        pthread_detach(tid);
    }

    close(listener);
    return 0;
}

void *client_handler(void *arg) {
    int client_fd = *(int *)arg;
    free(arg); 

    char buf[BUF_SIZE];
    int is_logged_in = 0; // Trạng thái đăng nhập cục bộ cho luồng này

    // Gửi yêu cầu đăng nhập
    char *prompt = "Hay dang nhap: <username> <password>\n";
    send(client_fd, prompt, strlen(prompt), 0);

    char out_file[64]; // file đầu ra kết quả

    while (1) {
        memset(buf, 0, BUF_SIZE);
        int ret = recv(client_fd, buf, sizeof(buf) - 1, 0);

        if (ret <= 0) {
            printf("Client %d disconnected\n", client_fd);
            break;
        }

        buf[ret] = '\0';
        
        // Loại bỏ ký tự thừa
        if (strlen(buf) > 0 && buf[strlen(buf) - 1] == '\n') {
            buf[strlen(buf) - 1] = '\0';
        }
        if (strlen(buf) > 0 && buf[strlen(buf) - 1] == '\r') {
            buf[strlen(buf) - 1] = '\0';
        }

        printf("Received from %d: %s\n", client_fd, buf);

        // Kiểm tra đăng nhập
        if (is_logged_in == 0) {
            char user[32], pass[32], tmp[64];
            int n = sscanf(buf, "%31s %31s %63s", user, pass, tmp);
            
            if (n != 2) {
                char *msg = "Sai cu phap. Hay dang nhap lai: <username> <password>\n";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                char cred[64];
                snprintf(cred, sizeof(cred), "%s %s", user, pass);

                int found = 0;
                char line[64];
                FILE *f = fopen("users.txt", "r");
                if (f) {
                    while (fgets(line, sizeof(line), f) != NULL) {
                        if (strlen(line) > 0 && line[strlen(line) - 1] == '\n')
                            line[strlen(line) - 1] = '\0';
                        if (strlen(line) > 0 && line[strlen(line) - 1] == '\r')
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
                    send(client_fd, msg, strlen(msg), 0);
                    is_logged_in = 1;
                } else {
                    char *msg = "Sai username hoac password. Hay dang nhap lai.\n";
                    send(client_fd, msg, strlen(msg), 0);
                }
            }
        } 
        // Đăng nhập thành công
        else {
            if (strcmp(buf, "exit") == 0) {
                send(client_fd, "Ban da dong ket noi!\n", 5, 0);
                break; // Thoát vòng lặp để đóng kết nối
            }

            char cmd[512];
            
            // Đặt tên file đầu ra riêng cho từng client
            snprintf(out_file, sizeof(out_file), "out_%d.txt", client_fd);
            snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", buf, out_file);
            
            system(cmd);

            // Đọc nội dung trong file và gửi cho client
            FILE *f = fopen(out_file, "rb");
            if (f) {
                int len;
                char file_buf[BUF_SIZE];
                while ((len = fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
                    send(client_fd, file_buf, len, 0);
                }
                fclose(f);
            }
            
        }
    }
    // Xóa file tạm sau khi đã gửi xong kết quả
    remove(out_file);
    close(client_fd);
    pthread_exit(NULL);
}