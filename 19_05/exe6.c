// Time_server : multithread
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 256

void *handle_client(void *arg);

int main() {
    int listener, client;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    
    // Thêm setsockopt để tránh lỗi "Address already in use" khi khởi động lại server nhanh
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind() failed");
        close(listener);
        exit(EXIT_FAILURE);
    }

    if (listen(listener, 5) < 0) {
        perror("Listen() failed");
        close(listener);
        exit(EXIT_FAILURE);
    }

    printf("Time_server dang chay tai port %d... \n", PORT);

    while (1) {
        client = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client < 0) {
            perror("Loi accept");
            continue;
        }

        printf("Co client moi ket noi tai fd = %d\n", client);

        int *client_sock = malloc(sizeof(int));
        *client_sock = client;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void*)client_sock) != 0) {
            perror("pthread_create() failed");
            close(client);
            free(client_sock);
        } else {
            pthread_detach(tid);
        }
    }

    close(listener);
    return 0;
}

void *handle_client(void *arg) {
    int client = *(int *)arg;
    free(arg);

    pthread_t tid = pthread_self();     // thread ID
    
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    // Lấy thời gian hiện tại của hệ thống
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    memset(buffer, 0, sizeof(buffer));

    while (1) {
        // Nhận dữ liệu từ client
        int bytes_read = recv(client, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read <= 0) {
            break; // Client ngắt kết nối hoặc có lỗi
        }
        
        // Xử lý ký tự cuối
        buffer[bytes_read] = '\0'; 
        buffer[strcspn(buffer, "\r\n")] = 0; 

        // Kiểm tra cú pháp
        if (strncmp(buffer, "GET_TIME ", 9) == 0) {
            
            char format_req[20];
            sscanf(buffer, "GET_TIME %19s", format_req); 
            memset(response, 0, sizeof(response));

            // So sánh và trả về định dạng tương ứng
            if (strcmp(format_req, "dd/mm/yyyy") == 0) {
                strftime(response, sizeof(response), "%d/%m/%Y\n", tm);
                printf("[Client TID = %lu]: %s", (unsigned long)tid, response);
            } 
            else if (strcmp(format_req, "dd/mm/yy") == 0) {
                strftime(response, sizeof(response), "%d/%m/%y\n", tm);
                printf("[Client TID = %lu]: %s", (unsigned long)tid, response);
            } 
            else if (strcmp(format_req, "mm/dd/yyyy") == 0) {
                strftime(response, sizeof(response), "%m/%d/%Y\n", tm);
                printf("[Client TID = %lu]: %s", (unsigned long)tid, response);
            } 
            else if (strcmp(format_req, "mm/dd/yy") == 0) {
                strftime(response, sizeof(response), "%m/%d/%y\n", tm);
                printf("[Client TID = %lu]: %s", (unsigned long)tid, response);
            } 
            else {
                // Sai định dạng format
                strcpy(response, "Loi: Format khong duoc ho tro. Cac format hop le: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy\n");
            }
        } else if (strncmp(buffer, "exit", 4) == 0) {
            break;
        } else {
            // Sai cú pháp lệnh
            strcpy(response, "Loi: Lenh khong hop le. Hay su dung cu phap: GET_TIME [format]\n");
        }

        // Gửi kết quả về cho client
        send(client, response, strlen(response), 0);
    }

    printf("[Client TID = %lu] da ngat ket noi!\n", (unsigned long)tid);

    // Đóng socket và thoát luồng
    close(client);
    pthread_exit(NULL);
}