#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>

// Hàm xác định loại nội dung (MIME type) dựa vào đuôi file
const char* get_mime_type(const char* path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".txt") == 0) return "text/plain; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(dot, ".mp4") == 0) return "video/mp4";
    return "application/octet-stream"; // Định dạng mặc định để trình duyệt tải về
}

// Giải mã URL để đọc được file có khoảng trắng
void url_decode(char *src, char *dest) {
    char *p = src;
    char code[3] = {0};
    while(*p) {
        if(*p == '%') {
            strncpy(code, ++p, 2);
            *dest++ = (char)strtol(code, NULL, 16);
            p += 2;
        } else {
            *dest++ = *p++;
        }
    }
    *dest = '\0';
}

// Xử lý yêu cầu từ người dùng
void process_request(int client_sock) {
    char buffer[4096];
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) { 
        close(client_sock); 
        return; }
    buffer[bytes_received] = '\0';

    char method[16], uri[2048];
    sscanf(buffer, "%s %s", method, uri);

    // Chỉ hỗ trợ phương thức GET
    if (strcmp(method, "GET") != 0) {
        char *response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        close(client_sock);
        return;
    }

    // Giải mã URI và tạo đường dẫn thực tế (Map '/' to './')
    char decoded_uri[2048];
    url_decode(uri, decoded_uri);
    
    char local_path[2048] = "."; // Thư mục gốc là nơi chạy server
    strcat(local_path, decoded_uri);

    // Lấy thông tin đường dẫn
    struct stat path_stat;
    if (stat(local_path, &path_stat) != 0) {
        char *response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 - Khong tim thay file!</h1>";
        send(client_sock, response, strlen(response), 0);
        close(client_sock);
        return;
    }

    // Thư mục
    if (S_ISDIR(path_stat.st_mode)) {
        DIR *dir = opendir(local_path);
        if (!dir) { close(client_sock); return; }

        char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"
                        "<html><head><title>File Server</title></head>"
                        "<body style='font-family: Arial, sans-serif;'>"
                        "<h2>Duyệt thư mục: %s</h2><hr><ul>";
        char html_buf[4096];
        sprintf(html_buf, header, decoded_uri);
        send(client_sock, html_buf, strlen(html_buf), 0);

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0) continue; // Bỏ qua thư mục hiện tại

            char item_path[4096];
            sprintf(item_path, "%s/%s", local_path, entry->d_name);
            struct stat item_stat;
            stat(item_path, &item_stat);

            char link[4096];
            // Xử lý dấu '/' cho URI
            char next_uri[3000];
            if (strcmp(decoded_uri, "/") == 0) sprintf(next_uri, "/%s", entry->d_name);
            else sprintf(next_uri, "%s/%s", decoded_uri, entry->d_name);

            // In đậm cho thư mục, In nghiêng cho file
            if (S_ISDIR(item_stat.st_mode)) {
                sprintf(link, "<li><a href=\"%s\"><b>%s/</b></a></li>", next_uri, entry->d_name);
            } else {
                sprintf(link, "<li><a href=\"%s\"><i>%s</i></a></li>", next_uri, entry->d_name);
            }
            send(client_sock, link, strlen(link), 0);
        }
        closedir(dir);

        char *footer = "</ul><hr></body></html>";
        send(client_sock, footer, strlen(footer), 0);
    } 
    // Tệp tin
    else if (S_ISREG(path_stat.st_mode)) {
        FILE *fp = fopen(local_path, "rb"); // Mở file ở chế độ nhị phân
        if (!fp) { close(client_sock); return; }

        // Gửi Header với MIME type tương ứng
        const char *mime = get_mime_type(local_path);
        char header[1024];
        sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n", mime, path_stat.st_size);
        send(client_sock, header, strlen(header), 0);

        // Đọc và gửi file theo từng đoạn (Chunking)
        char file_buf[8192];
        size_t bytes_read;
        while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), fp)) > 0) {
            send(client_sock, file_buf, bytes_read, 0);
        }
        fclose(fp);
    }

    close(client_sock);
}

void* client_handler(void* arg) {
    int client_sock = *(int*)arg;
    free(arg); 
    process_request(client_sock);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    bind(listener, (struct sockaddr*)&addr, sizeof(addr));
    listen(listener, 10);
    printf("HTTP File Server đang chạy tại http://localhost:8080 ...\n");

    while(1) {
        int client = accept(listener, NULL, NULL);
        if (client != -1) {
            int *pclient = malloc(sizeof(int));
            *pclient = client;
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, client_handler, pclient) == 0) {
                pthread_detach(thread_id);
            }
        }
    }
    return 0;
}