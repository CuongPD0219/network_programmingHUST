#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_HOST "lebavui.io.vn" 
#define SERVER_PORT 21
#define FTP_USER "user_20235288"
#define FTP_PASS "528824"

// Hàm tạo kết nối TCP
int connect_to_server(const char *hostName, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo hints, *res;
    char port_str[16];

    // Khởi tạo bộ lọc cho getaddrinfo
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // Dùng IPv4
    hints.ai_socktype = SOCK_STREAM; // Giao thức TCP

    sprintf(port_str, "%d", port);

    // Phân giải tên miền
    if (getaddrinfo(hostName, port_str, &hints, &res) != 0) {
        printf("Lỗi phân giải tên miền hoặc IP\n");
        exit(1);
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("Kết nối thất bại");
        freeaddrinfo(res); // Giải phóng bộ nhớ trước khi thoát
        exit(1);
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    printf("Đã kết nối tới IP: %s | Port: %d\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));

    freeaddrinfo(res); 
    return sock;
}

// Hàm gửi lệnh FTP
void send_cmd(int sock, const char *cmd) {
    // printf(">> Gõ lệnh yêu cầu gửi đến server: ");
    send(sock, cmd, strlen(cmd), 0);
    printf(">> %s", cmd);
}

// Hàm đọc phản hồi từ Server
void read_resp(int sock, char *buffer, int size) {
    memset(buffer, 0, size);
    recv(sock, buffer, size - 1, 0);
    printf("<< %s", buffer);
}

// Hàm phân tích IP và Port từ lệnh PASV
int enter_pasv(int control_sock, char *data_ip) {
    char buffer[1024];
    send_cmd(control_sock, "PASV\r\n");
    read_resp(control_sock, buffer, sizeof(buffer));

    int h1, h2, h3, h4, p1, p2;
    // Phân tích cú pháp : 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
    char *start = strchr(buffer, '(');
    if (start) {
        sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
        sprintf(data_ip, "%d.%d.%d.%d", h1, h2, h3, h4);
        return (p1 * 256) + p2; // Tính toán port dữ liệu
    }
    return -1;
}

// Hàm đảo ngược chuỗi
void reverse_string(char *str) {
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

int main() {
    char buffer[4096];
    char data_ip[32];
    int data_port;

    // 1. Kết nối kênh điều khiển 
    printf("--- KẾT NỐI SERVER ---\n");
    int control_sock = connect_to_server(SERVER_HOST, SERVER_PORT);     // socket quản lý kết nối từ client đến server
    read_resp(control_sock, buffer, sizeof(buffer)); 

    // 2. Đăng nhập
    char cmd[512];
    sprintf(cmd, "USER %s\r\n", FTP_USER);
    send_cmd(control_sock, cmd);
    read_resp(control_sock, buffer, sizeof(buffer)); 

    sprintf(cmd, "PASS %s\r\n", FTP_PASS);
    send_cmd(control_sock, cmd);
    read_resp(control_sock, buffer, sizeof(buffer)); 

    // 3. Lấy danh sách file 
    printf("\n--- LẤY DANH SÁCH FILE ---\n");
    data_port = enter_pasv(control_sock, data_ip);
    int data_sock = connect_to_server(data_ip, data_port);
    
    // Gửi lệnh LIST 
    send_cmd(control_sock, "LIST\r\n");
    read_resp(control_sock, buffer, sizeof(buffer)); 

    char raw_list_data[256] = {0};
    recv(data_sock, raw_list_data, sizeof(raw_list_data), 0);  
    close(data_sock);
    read_resp(control_sock, buffer, sizeof(buffer)); 

    //Xóa ký tự xuống dòng \r\n ở cuối chuỗi thô
    raw_list_data[strcspn(raw_list_data, "\r\n")] = 0; 
    printf("Dữ liệu LIST nhận được: '%s'\n", raw_list_data);

    // Tìm vị trí tên file ở sau dấu cách cuối cùng
    char *last_space = strrchr(raw_list_data, ' ');
    char filename[256] = {0};
    strcpy(filename, last_space + 1);
    printf("=> Đã bóc tách tên file chuẩn: '%s'\n", filename);

    // 4. Tải nội dung file question
    printf("\n--- TẢI NỘI DUNG FILE ---\n");
    data_port = enter_pasv(control_sock, data_ip);
    data_sock = connect_to_server(data_ip, data_port);

    sprintf(cmd, "RETR %s\r\n", filename);
    send_cmd(control_sock, cmd);
    read_resp(control_sock, buffer, sizeof(buffer));

    char file_content[1024] = {0};
    recv(data_sock, file_content, sizeof(file_content), 0);
    close(data_sock);
    read_resp(control_sock, buffer, sizeof(buffer));
    
    printf("Nội dung gốc: %s\n", file_content);

    // 5. Xử lý đảo ngược và tạo tên file answer
    printf("\n--- XỬ LÝ DỮ LIỆU ---\n");
    reverse_string(file_content);
    printf("Nội dung đảo ngược: %s\n", file_content);

    // Xử lý phần đuôi file và tạo tên file answer
    char answer_filename[256];
    char *tail = strchr(filename, '_');
    tail++;
    strcpy(answer_filename, "answer_");
    strcat(answer_filename, tail);

    // 6. Gửi file answer lên server
    printf("\n--- UPLOAD FILE ANSWER ---\n");
    data_port = enter_pasv(control_sock, data_ip);
    data_sock = connect_to_server(data_ip, data_port);

    sprintf(cmd, "STOR %s\r\n", answer_filename);
    send_cmd(control_sock, cmd);
    read_resp(control_sock, buffer, sizeof(buffer));

    send(data_sock, file_content, strlen(file_content), 0); 
    close(data_sock);
    read_resp(control_sock, buffer, sizeof(buffer)); 

    // 7. Thoát
    printf("\n--- HOÀN TẤT ---\n");
    send_cmd(control_sock, "QUIT\r\n");
    read_resp(control_sock, buffer, sizeof(buffer));
    close(control_sock);

    return 0;
}