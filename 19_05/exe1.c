#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

#define PORT 8000
#define DIR_PATH "./shared_files" // Thư mục được thiết lập trên server
#define BUFFER_SIZE 1024

// Hàm xử lý từng client bằng luồng riêng biệt
void *client_handler(void *);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind() failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listen() failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    printf("Thu muc hien tai %s...\n", DIR_PATH);
    printf("Server dang lang nghe tren port %d...\n", PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept() failed");
            continue;
        }

        printf("[Client: %d] moi ket noi!\n", client_sock);

        // Cấp phát bộ nhớ cho socket descriptor để truyền vào luồng
        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        // Tạo luồng mới để xử lý client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, (void *)new_sock) < 0) {
            perror("Khong the tao luong");
            free(new_sock);
            close(client_sock);
        }

        // Tách luồng để hệ thống tự thu hồi tài nguyên sau khi luồng kết thúc
        pthread_detach(thread_id);
    }

    close(server_sock);
    return 0;
}

void *client_handler(void *arg) {
    int client_sock = *(int *)arg;
    free(arg); // Giải phóng bộ nhớ con trỏ được truyền vào

    DIR *d;
    struct dirent *dir;
    int file_count = 0;
    char temp_list[4096] = "";
    char response[8192] = "";

    // Mở thư mục và đếm số lượng file
    d = opendir(DIR_PATH);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {
                file_count++;
                strcat(temp_list, dir->d_name);
                strcat(temp_list, "\r\n");
            }
        }
        closedir(d);
    } else {
        perror("Khong the mo thu muc hien co");
    }

    // Gửi danh sách file cho client khi mới được chấp nhận 
    if (file_count == 0) {
        // Nếu không có file nào, gửi thông báo lỗi và đóng kết nối 
        char *error_msg = "ERROR No files to download\r\n";
        send(client_sock, error_msg, strlen(error_msg), 0);
        printf("[Client: %d] dong ket noi!\n", client_sock);
        close(client_sock);
        pthread_exit(NULL);
    } else {
        // Gửi chuỗi OK N\r\n kèm danh sách file, kết thúc bởi \r\n\r\n 
        snprintf(response, sizeof(response), "OK %d\r\n%s\r\n", file_count, temp_list);
        send(client_sock, response, strlen(response), 0);
    }

    // Xử lý yêu cầu tải file từ client
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            printf("[Client: %d] dong ket noi!\n", client_sock);
            break; // Client ngắt kết nối
        }

        buffer[strcspn(buffer, "\r\n")] = 0;

        char filepath[2048];
        snprintf(filepath, sizeof(filepath), "%s/%s", DIR_PATH, buffer);

        // Client gửi tên file để tải về, server kiểm tra file tồn tại 
        FILE *fp = fopen(filepath, "rb");
        if (fp == NULL) {
            // Nếu file không tồn tại, gửi thông báo lỗi và yêu cầu gửi lại tên 
            char *err_msg = "That bai! Khong tim thay file, vui long gui lai theo dung cu phap: <ten_file>.<dinh_dang>\r\n";
            send(client_sock, err_msg, strlen(err_msg), 0);
        } else {
            // Nếu file tồn tại, tính kích thước file
            fseek(fp, 0, SEEK_END);
            long file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            // Gửi chuỗi OK N\r\n (N là kích thước file) 
            char ok_msg[256];
            snprintf(ok_msg, sizeof(ok_msg), "OK %ld\r\n", file_size);
            send(client_sock, ok_msg, strlen(ok_msg), 0);

            // Sau đó gửi nội dung file cho client 
            char file_buf[BUFFER_SIZE];
            size_t bytes_read;
            while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), fp)) > 0) {
                send(client_sock, file_buf, bytes_read, 0);
            }
            
            fclose(fp);
            break; 
        }
    }

    printf("[Client: %d] dong ket noi!\n", client_sock);
    close(client_sock);
    pthread_exit(NULL);
}
