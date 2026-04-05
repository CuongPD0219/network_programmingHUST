#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT      9000
#define SERVER_IP "127.0.0.1"
#define BUF_SIZE  256

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buf[BUF_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Không thể kết nối đến server");
        return 1;
    }

    printf("=== Kết nối thành công đến Email Server ===\n");

    while (1) {
        // Nhận câu hỏi từ server
        memset(buf, 0, BUF_SIZE);
        int n = recv(sock, buf, BUF_SIZE - 1, 0);
        if (n <= 0) break;
        printf("%s", buf);
        fflush(stdout);

        // Nếu server đã trả kết quả (có "Email cua ban") thì thoát
        if (strstr(buf, "Email cua ban") != NULL) break;

        // Nhập dữ liệu từ bàn phím và gửi lên server
        memset(buf, 0, BUF_SIZE);
        if (fgets(buf, BUF_SIZE, stdin) == NULL) break;
        send(sock, buf, strlen(buf), 0);
    }

    close(sock);
    return 0;
}