#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <ctype.h>

#define PORT     9000
#define MAX_CLIENTS 10
#define BUF_SIZE 256

// Trạng thái của mỗi client
typedef enum {
    STATE_ASK_NAME = 0,  // Đang chờ nhận họ tên
    STATE_ASK_MSSV,      // Đang chờ nhận MSSV
    STATE_DONE           // Đã xử lý xong
} ClientState;

typedef struct {
    int fd;
    ClientState state;
    char hoten[BUF_SIZE];
    char mssv[BUF_SIZE];
} Client;

// Xóa khoảng trắng 2 đầu chuỗi
void trim(char *s) {
    // lọc bên phải
    int len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' '))
        s[--len] = '\0';
    // lọc bên trái
    int start = 0;
    while (s[start] == ' ') start++;
    if (start > 0) memmove(s, s + start, strlen(s) - start + 1);
}


// Tạo email theo chuẩn: <tên>.<viết tắt họ đệm><MSSV bỏ 2 số đầu>@sis.hust.edu.vn
void build_email(const char *hoten, const char *mssv, char *email, int email_size) {
    // Tách họ tên thành các từ, chuyển về lowercase
    char words[10][64];
    int nwords = 0;
    char tmp[BUF_SIZE];
    strncpy(tmp, hoten, BUF_SIZE - 1);

    char *tok = strtok(tmp, " ");
    while (tok && nwords < 10) {
        int j = 0;
        while (tok[j] && j < 63) {
            words[nwords][j] = tolower((unsigned char)tok[j]);
            j++;
        }
        words[nwords][j] = '\0';
        nwords++;
        tok = strtok(NULL, " ");
    }

    if (nwords == 0) {
        snprintf(email, email_size, "unknown@sis.hust.edu.vn");
        return;
    }

    // Tên = từ cuối cùng
    char *ten = words[nwords - 1];

    // Tách chữ cái đầu
    char vt[16] = {0};
    int vi = 0;
    for (int i = 0; i < nwords - 1 && vi < 15; i++) {
        if (strlen(words[i]) > 0)
            vt[vi++] = words[i][0];
    }
    vt[vi] = '\0';

    // thiết lập MSSV
    char mssv_clean[BUF_SIZE] = {0};
    int mi = 0;
    for (int i = 0; mssv[i] && mi < BUF_SIZE - 1; i++) {
        if (!isspace((unsigned char)mssv[i]))
            mssv_clean[mi++] = mssv[i];
    }
    // Bỏ 2 ký tự đầu nếu MSSV đủ dài
    const char *mssv_short = (strlen(mssv_clean) > 2) ? mssv_clean + 2 : mssv_clean;

    snprintf(email, email_size, "%s.%s%s@sis.hust.edu.vn", ten, vt, mssv_short);
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    // Tạo socket server
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Đặt non-blocking cho server socket
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, MAX_CLIENTS);

    printf("=== Email Server (non-blocking, select) ===\n");
    printf("Đang lắng nghe cổng %d...\n\n", PORT);

    fd_set read_fds;
    char buf[BUF_SIZE];

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &read_fds);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) { perror("select"); break; }

        // Có client mới kết nối 
        if (FD_ISSET(server_fd, &read_fds)) {
            int new_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
            if (new_fd >= 0) {
                fcntl(new_fd, F_SETFL, O_NONBLOCK);
                printf("[+] Client kết nối: %s:%d (fd=%d)\n",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port), new_fd);

                // Tìm slot trống
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == -1) {
                        clients[i].fd    = new_fd;
                        clients[i].state = STATE_ASK_NAME;
                        memset(clients[i].hoten, 0, BUF_SIZE);
                        memset(clients[i].mssv,  0, BUF_SIZE);

                        // Hỏi họ tên ngay khi kết nối
                        send(new_fd, "Nhap ho ten: ", 13, 0);
                        break;
                    }
                }
            }
        }

        // Xử lý dữ liệu từ từng client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;
            if (fd == -1 || !FD_ISSET(fd, &read_fds)) continue;

            memset(buf, 0, BUF_SIZE);
            int n = recv(fd, buf, BUF_SIZE - 1, 0);

            if (n <= 0) {
                // Client ngắt kết nối
                printf("[-] Client fd=%d ngắt kết nối.\n", fd);
                close(fd);
                clients[i].fd = -1;
                continue;
            }

            trim(buf);

            if (clients[i].state == STATE_ASK_NAME) {
                // Lưu họ tên, hỏi MSSV
                strncpy(clients[i].hoten, buf, BUF_SIZE - 1);
                printf("    fd=%d | Ho ten: \"%s\"\n", fd, clients[i].hoten);
                send(fd, "Nhap MSSV: ", 11, 0);
                clients[i].state = STATE_ASK_MSSV;

            } else if (clients[i].state == STATE_ASK_MSSV) {
                // Lưu MSSV, tạo email và gửi về
                strncpy(clients[i].mssv, buf, BUF_SIZE - 1);
                printf("    fd=%d | MSSV: \"%s\"\n", fd, clients[i].mssv);

                char email[BUF_SIZE];
                build_email(clients[i].hoten, clients[i].mssv, email, BUF_SIZE);

                char reply[BUF_SIZE * 2];
                snprintf(reply, BUF_SIZE * 2, "Email cua ban: %s\n", email);
                send(fd, reply, strlen(reply), 0);

                printf("    fd=%d | Email: %s\n", fd, email);

                clients[i].state = STATE_DONE;
                // Đóng kết nối sau khi đã trả email
                close(fd);
                clients[i].fd = -1;
                printf("[x] Đã đóng kết nối fd=%d.\n\n", fd);
            }
        }
    }

    close(server_fd);
    return 0;
}