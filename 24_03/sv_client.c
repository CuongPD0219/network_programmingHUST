#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define BUFFER_SIZE 512

/*
 * Cấu trúc thông tin sinh viên được đóng gói thành một chuỗi văn bản: MSSV HoTen NgaySinh DiemTB\n
 */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_IP> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    int port              = atoi(argv[2]);

    // Tạo socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Cấu hình địa chỉ server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Kết nối server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to sv_server at %s:%d\n", server_ip, port);

    // Nhập thông tin sinh viên
    char mssv[32], hoten[128], ngaysinh[32];
    float diem;

    printf("Enter student ID   : ");
    scanf("%31s", mssv);
    getchar(); // bỏ '\n' còn lại trong buffer

    printf("Enter full name    : ");
    fgets(hoten, sizeof(hoten), stdin);
    hoten[strcspn(hoten, "\n")] = '\0'; // xoá newline

    printf("Enter date of birth (DD-MM-YYYY): ");
    scanf("%31s", ngaysinh);

    printf("Enter GPA          : ");
    scanf("%f", &diem);

    // Đóng gói thành một dòng: MSSV HoTen NgaySinh DiemTB
    char packet[BUFFER_SIZE];
    snprintf(packet, sizeof(packet), "%s %s %s %.2f\n",
             mssv, hoten, ngaysinh, diem);

    // Gửi đến server
    if (send(sockfd, packet, strlen(packet), 0) < 0) {
        perror("send");
    } else {
        printf("Data sent: %s", packet);
    }

    close(sockfd);
    return 0;
}