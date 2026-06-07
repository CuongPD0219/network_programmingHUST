#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h> 

// Xử lý yêu cầu, tính toán và trả về HTML
void process_request(int client_sock) {
    char buffer[4096];
    int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_sock);
        return;
    }
    buffer[bytes_received] = '\0';

    char method[16], path[2048];
    sscanf(buffer, "%s %s", method, path);
    printf("[Thread] Đang xử lý yêu cầu %s cho đường dẫn: %s\n", method, path);

    // Tách yêu cầu và tham số
    char *param_string = NULL;
    if (strcmp(method, "GET") == 0) {
        param_string = strchr(path, '?');
        if (param_string) param_string++; 
    } else if (strcmp(method, "POST") == 0) {
        param_string = strstr(buffer, "\r\n\r\n");
        if (param_string) param_string += 4; 
    }

    double a = 0, b = 0, result = 0;
    char op[16] = "";
    char result_text[256] = "Chưa nhận đủ tham số. Hãy gửi a, b và op (add, sub, mul, div).";

    // Tách các tham số và tính toán
    if (param_string) {
        char *a_ptr = strstr(param_string, "a=");
        char *b_ptr = strstr(param_string, "b=");
        char *op_ptr = strstr(param_string, "op=");

        if (a_ptr && b_ptr && op_ptr) {
            a = atof(a_ptr + 2);
            b = atof(b_ptr + 2);
            sscanf(op_ptr + 3, "%15[^&\n \r]", op);

            if (strcmp(op, "add") == 0) { result = a + b; sprintf(result_text, "%.2f + %.2f = %.2f", a, b, result); }
            else if (strcmp(op, "sub") == 0) { result = a - b; sprintf(result_text, "%.2f - %.2f = %.2f", a, b, result); }
            else if (strcmp(op, "mul") == 0) { result = a * b; sprintf(result_text, "%.2f * %.2f = %.2f", a, b, result); }
            else if (strcmp(op, "div") == 0) {
                if (b != 0) { result = a / b; sprintf(result_text, "%.2f / %.2f = %.2f", a, b, result); }
                else { strcpy(result_text, "Lỗi: Toán học không cho phép chia cho 0!"); }
            } else { strcpy(result_text, "Lỗi: Toán tử không hợp lệ."); }
        }
    }

    // Nội dung thẻ HTML kết quả
    char response_body[1024];
    sprintf(response_body,
            "<html><head><meta charset=\"UTF-8\"><title>Server</title></head>"
            "<body style=\"font-family: Arial; padding: 20px;\">"
            "<h2>IT4060 - Kết quả phép tính (Multi-threaded Server)</h2>"
            "<p><strong>Phương thức nhận:</strong> %s</p>"
            "<p><strong>Kết quả:</strong> <span style=\"color:green; font-size: 20px;\">%s</span></p>"
            "</body></html>\n",
            method, result_text);

    // Nội dung response
    char response[2048];
    sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close\r\n\r\n"
            "%s", strlen(response_body), response_body);

    send(client_sock, response, strlen(response), 0);
    close(client_sock);
    printf("[Thread] <- ĐÃ NGẮT kết nối Socket FD = %d thành công!\n", client_sock);
}

void * client_handler(void* arg) {
    int client_sock = *(int*)arg;

    free(arg);  //giải phóng vùng nhớ

    // Thực hiện xử lý yêu cầu HTTP
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

    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind() failed");
        return 1;
    }
    
    listen(listener, 10);
    printf("Server đang chạy tại http://localhost:8080 ...\n");

    while(1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client = accept(listener, (struct sockaddr*)&client_addr, &client_len);
        
        if (client != -1) {
            printf("[Server] Nhận kết nối thành công! Socket FD = %d\n", client);

            int *pclient = malloc(sizeof(int));
            *pclient = client;

            pthread_t thread_id;
            // Tạo luồng mới và chuyển giao quyền xử lý socket cho hàm client_handler
            if (pthread_create(&thread_id, NULL, client_handler, pclient) != 0) {
                perror("Không thể tạo luồng con");
                close(client);
                free(pclient);
            } else {
                pthread_detach(thread_id);
            }
        }
    }

    close(listener);
    return 0;
}