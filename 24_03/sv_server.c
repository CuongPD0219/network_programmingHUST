#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 512
#define BACKLOG 5

/*
 * Lấy timestamp hiện tại dạng "YYYY-MM-DD HH:MM:SS"
 */
static void get_timestamp(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <port> <log_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    const char *log_file = argv[2];

    // Tạo socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd, BACKLOG) < 0)
    {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("sv_server listening on port %d, logging to %s\n", port, log_file);

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
        if (connfd < 0)
        {
            perror("accept");
            continue;
        }

        // Lấy địa chỉ IP client
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        // Lấy timestamp lúc client kết nối
        char timestamp[32];
        get_timestamp(timestamp, sizeof(timestamp));

        // Nhận dữ liệu từ client
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = recv(connfd, buffer, sizeof(buffer) - 1, 0);

        if (n > 0)
        {
            // Xoá newline cuối nếu có (để ghép vào log line gọn hơn)
            buffer[strcspn(buffer, "\n")] = '\0';

            // Dòng log: <IP> <timestamp> <dữ liệu sinh viên>
            char log_line[BUFFER_SIZE + 64];
            snprintf(log_line, sizeof(log_line), "%s %s %s\n",
                     client_ip, timestamp, buffer);

            // In ra màn hình
            printf("%s", log_line);

            // Ghi vào file log
            FILE *fp = fopen(log_file, "a");
            if (fp)
            {
                fputs(log_line, fp);
                fclose(fp);
            }
            else
            {
                perror("fopen log_file");
            }
        }
        else if (n < 0)
        {
            perror("recv");
        }

        close(connfd);
    }

    close(listenfd);
    return 0;
}