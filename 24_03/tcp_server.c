#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define BACKLOG     5

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <port> <greeting_file> <log_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port              = atoi(argv[1]);
    const char *greet_file = argv[2];
    const char *log_file   = argv[3];

    // Đọc xâu chào từ file
    FILE *fp = fopen(greet_file, "r");
    if (!fp) {
        perror("fopen greeting_file");
        exit(EXIT_FAILURE);
    }
    char greeting[BUFFER_SIZE];
    memset(greeting, 0, sizeof(greeting));
    fread(greeting, 1, sizeof(greeting) - 1, fp);
    fclose(fp);

    // Tạo socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Cho phép tái sử dụng cổng
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(listenfd, BACKLOG) < 0) {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("tcp_server listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
        if (connfd < 0) {
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Client connected: %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // Gửi xâu chào đến client
        if (send(connfd, greeting, strlen(greeting), 0) < 0) {
            perror("send greeting");
        }

        // Mở file log để ghi nội dung client gửi đến (append)
        FILE *logfp = fopen(log_file, "a");
        if (!logfp) {
            perror("fopen log_file");
            close(connfd);
            continue;
        }

        // Nhận và ghi dữ liệu từ client
        char buffer[BUFFER_SIZE];
        ssize_t n;
        while ((n = recv(connfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[n] = '\0';
            printf("Received: %s", buffer);
            fwrite(buffer, 1, n, logfp);
        }

        if (n < 0) {
            perror("recv");
        }

        fclose(logfp);
        close(connfd);
        printf("Client %s disconnected.\n", client_ip);
    }

    close(listenfd);
    return 0;
}