#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define PORT 8080
#define NUM_WORKERS 5 // Số lượng tiến trình con  tạo sẵn

// Hàm xử lý của mỗi  tiến trình con
void worker_process(int listener, int worker_id) {
    int client_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buf[1024];

    printf("Worker %d (PID: %d) dang cho ket noi...\n", worker_id, getpid());

    while (1) {
        client_sock = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("\n[Worker PID: %d] New client connected: %s:%d\n", getpid(), inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Nhận dữ liệu từ client
        int ret = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (ret > 0) {
            buf[ret] = '\0'; // Đặt ký tự kết thúc chuỗi
            
            // In request ra màn hình 
            puts(buf); 

            // Trả lại kết quả cho client
            char body[512];
            snprintf(body, sizeof(body),
                "<html><body>"
                "<h1>Xin chao cac ban!</h1>"
                "<p>Process PID: <b>%d</b> (Worker #%d) da xu ly yeu cau nay.</p>"
                "</body></html>",
                getpid(), worker_id);
        
            char header[256];
            snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Content-Length: %zu\r\n"
                "Connection: close\r\n"
                "\r\n",
                strlen(body));
        
            send(client_sock, header, strlen(header), 0);
            send(client_sock, body,   strlen(body),   0);

        }

        close(client_sock);
    }
}

int main() {
    int listener;
    struct sockaddr_in server_addr;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("Khong the tao socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind that bai");
        close(listener);
        exit(EXIT_FAILURE);
    }

    if (listen(listener, 10) < 0) {
        perror("Listen that bai");
        close(listener);
        exit(EXIT_FAILURE);
    }

    printf("HTTP Server (Preforking) dang chay tai port %d...\n", PORT);
    printf("Tao pool gom %d workers...\n", NUM_WORKERS);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            // Đang ở trong tiến trình con 
            worker_process(listener, i + 1);
            exit(0); 
        } else if (pid < 0) {
            perror("Loi tao tien trinh (fork)");
        }
    }

    // Tiến trình cha (Master process) chỉ làm nhiệm vụ quản lý, đợi các con (hoặc chạy vòng lặp vô hạn)
    for (int i = 0; i < NUM_WORKERS; i++) {
        wait(NULL);
    }

    close(listener);
    return 0;
}