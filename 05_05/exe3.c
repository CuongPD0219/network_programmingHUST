#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8080
#define BUFFER_SIZE 256

// xử lý yêu cầu từ client 
void handle_client(int client) {
    
}

void signal_handler(int sig) {
    int pid = wait(NULL);
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        printf("Child process terminated: %d\n", pid);
    }
}

int main() {
    int listener, client;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    signal(SIGCHLD, signal_handler);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("Khong the tao socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind that bai");
        close(listener);
        exit(EXIT_FAILURE);
    }

    if (listen(listener, 5) < 0) {
        perror("Listen that bai");
        close(listener);
        exit(EXIT_FAILURE);
    }

    printf("Time_server dang chay tai port %d...\n", PORT);

    while (1) {
        client = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client < 0) {
            perror("Loi accept");
            continue;
        }

        printf("Co client moi ket noi tai fd = %d\n", client);

        pid_t pid = fork();

        if (pid == 0) {
            close(listener); 
            
            char buffer[BUFFER_SIZE];
            char response[BUFFER_SIZE];
    
            // lấy thời gian hiện tại của hệ thống
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);

            memset(buffer, 0, sizeof(buffer));

            while(1){
                // nhận dữ liệu từ client
                int bytes_read = recv(client, buffer, sizeof(buffer) - 1, 0);

                if (bytes_read <=  0) break;
                else {
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    // kiểm tra cú pháp
                    if (strncmp(buffer, "GET_TIME ", 9) == 0) {
                        
                        // trích xuất phần định dạng (format) nằm sau chữ "GET_TIME "
                        char format_req[20] ;
                        sscanf(buffer, "GET_TIME %s", format_req);
                        memset(response, 0, sizeof(response));

                        // so sánh và trả về định dạng tương ứng
                        if (strcmp(format_req, "dd/mm/yyyy") == 0) {
                            strftime(response, sizeof(response), "%d/%m/%Y\n", tm);
                            printf("[Client pid = %d]: %s\n", getpid(), response);

                        } 
                        else if (strcmp(format_req, "dd/mm/yy") == 0) {
                            strftime(response, sizeof(response), "%d/%m/%y\n", tm);
                            printf("[Client pid = %d]: %s\n", getpid(), response);
                        } 
                        else if (strcmp(format_req, "mm/dd/yyyy") == 0) {
                            strftime(response, sizeof(response), "%m/%d/%Y\n", tm);
                            printf("[Client pid = %d]: %s\n", getpid(), response);
                        } 
                        else if (strcmp(format_req, "mm/dd/yy") == 0) {
                            strftime(response, sizeof(response), "%m/%d/%y\n", tm);
                            printf("[Client pid = %d]: %s\n", getpid(), response);
                        } 
                        else {
                            // sai định dạng format
                            strcpy(response, "Loi: Format khong duoc ho tro. Cac format hop le: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy\n");
                        }
                    } else if(strncmp(buffer, "exit", 4) ==0) {
                        break;
                    }else{
                        // sai cú pháp lệnh
                        strcpy(response, "Loi: Lenh khong hop le. Hay su dung cu phap: GET_TIME [format]\n");
                    }

                    // Gửi kết quả về cho client
                    send(client, response, strlen(response), 0);
                }
            }

            char notification[100] ;
            sprintf(notification, "[Client pid = %d] da ngat ket noi!\n", getpid());
            printf("%s", notification);

            close(client);
            exit(0);    
        } else if (pid > 0) {
            close(client);
        } else {
            perror("Loi tao tien trinh (fork)");
        }
    }

    close(listener);
    return 0;
}