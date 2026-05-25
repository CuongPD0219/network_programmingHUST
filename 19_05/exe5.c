// HTTP_server : multithread
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define NUM_WORKERS 5 // Số lượng luồng (thread) tạo sẵn

// Cấu trúc dùng để truyền tham số vào luồng con
typedef struct {
    int listener;
    int worker_id;
} ThreadArgs;

// Hàm xử lý của mỗi luồng
void *worker_thread(void *arg);

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
        perror("Bind() failed");
        close(listener);
        exit(EXIT_FAILURE);
    }

    if (listen(listener, 10) < 0) {
        perror("Listen() failed");
        close(listener);
        exit(EXIT_FAILURE);
    }

    printf("HTTP Server dang chay tai port %d...\n", PORT);
    printf("Tao pool gom %d threads...\n", NUM_WORKERS);

    pthread_t threads[NUM_WORKERS];

    for (int i = 0; i < NUM_WORKERS; i++) {
        // Cấp phát động tham số để truyền vào luồng an toàn (tránh race condition)
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->listener = listener;
        args->worker_id = i + 1;

        if (pthread_create(&threads[i], NULL, worker_thread, (void*)args) != 0) {
            perror("pthread_create() failed");
        }
    }

    // Luồng chính (Master thread) chờ các luồng con (không bắt buộc vì các luồng chạy vô hạn)
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(threads[i], NULL);
    }

    close(listener);
    return 0;
}

void *worker_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int listener = args->listener;
    int worker_id = args->worker_id;
    free(arg);

    int client_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buf[1024];

    pthread_t tid = pthread_self();

    printf("Worker %d (TID: %lu) dang cho ket noi...\n", worker_id, (unsigned long)tid);

    while (1) {
        client_sock = accept(listener, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept() failed");
            continue;
        }

        printf("\n[Worker TID: %lu] New client connected: %s:%d\n", 
               (unsigned long)tid, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Nhận dữ liệu từ client
        int ret = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (ret > 0) {
            buf[ret] = '\0'; 
            
            puts(buf); 

            // Trả lại kết quả cho client
            char body[512];
            snprintf(body, sizeof(body),
                "<html><body>"
                "<h1>Xin chao cac ban!</h1>"
                "<p>Thread TID: <b>%lu</b> (Worker #%d) da xu ly yeu cau nay.</p>"
                "</body></html>\r\n",
                (unsigned long)tid, worker_id);
        
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
    
    pthread_exit(NULL);
}