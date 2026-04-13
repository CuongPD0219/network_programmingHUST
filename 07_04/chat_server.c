#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>

#define PORT        9000
#define MAX_CLIENTS 10
#define BUF_SIZE    1024

typedef enum {
    STATE_WAIT_ID = 0,  // Chờ client gửi "id: name"
    STATE_CHAT           // Đã xác thực, tham gia chat
} ClientState;

typedef struct {
    int  fd;
    ClientState state;
    char id[64];    // client_id
    char name[64];  // client_name
} Client;

Client clients[MAX_CLIENTS];
int    nclient = 0;

// Lấy timestamp hiện tại dạng "2024/01/15 02:30:00PM"
void get_timestamp(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y/%m/%d %I:%M:%S%p", t);
}

// Trim \r\n cuối chuỗi
void trim_crlf(char *s) {
    int len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r'))
        s[--len] = '\0';
}

// Tìm slot trống trong mảng clients
int find_free_slot() {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd == -1) return i;
    return -1;
}

// Kiểm tra id đã tồn tại chưa
int id_exists(const char *id) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd != -1 && strcmp(clients[i].id, id) == 0)
            return 1;
    return 0;
}

// Quang ba tin nhắn đến tất cả client đang chat, trừ sender (fd = -1 để gửi tất cả)
void broadcast(const char *msg, int sender_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) continue;
        if (clients[i].state != STATE_CHAT) continue;
        if (clients[i].fd == sender_fd) continue;
        send(clients[i].fd, msg, strlen(msg), 0);
    }
}

// Xóa client khỏi danh sách
void remove_client(int idx) {
    printf("[-] Client '%s' (fd=%d) ngat ket noi.\n",
           clients[idx].id[0] ? clients[idx].id : "?", clients[idx].fd);

    // Thông báo cho phòng chat nếu đã vào chat
    if (clients[idx].state == STATE_CHAT) {
        char msg[BUF_SIZE * 2];
        char ts[64]; get_timestamp(ts, sizeof(ts));
        snprintf(msg, BUF_SIZE, "%s [server]: %s da roi phong chat.\n",
                 ts, clients[idx].id);
        broadcast(msg, clients[idx].fd);
    }

    close(clients[idx].fd);
    clients[idx].fd    = -1;
    clients[idx].state = STATE_WAIT_ID;
    clients[idx].id[0] = '\0';
    clients[idx].name[0] = '\0';
}

int main() {
    // Khởi tạo mảng clients
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, MAX_CLIENTS);

    printf("=== Chat Server ===\n");
    printf("Lang nghe cong %d...\n\n", PORT);

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

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select"); break;
        }

        // Kết nối mới 
        if (FD_ISSET(server_fd, &read_fds)) {
            int new_fd = accept(server_fd,
                                (struct sockaddr*)&client_addr, &addr_len);
            if (new_fd >= 0) {
                int slot = find_free_slot();
                if (slot == -1) {
                    send(new_fd, "Server day, thu lai sau.\n", 25, 0);
                    close(new_fd);
                } else {
                    clients[slot].fd    = new_fd;
                    clients[slot].state = STATE_WAIT_ID;
                    clients[slot].id[0] = '\0';
                    clients[slot].name[0] = '\0';

                    printf("[+] Ket noi moi: %s:%d (fd=%d)\n",
                           inet_ntoa(client_addr.sin_addr),
                           ntohs(client_addr.sin_port), new_fd);

                    send(new_fd, "Chao mung! Gui dinh danh theo cu phap: ID: ten\n", strlen("Chao mung! Gui dinh danh theo cu phap: ID: ten\n"), 0);
                    send(new_fd, "Vi du: abc: NguyenVanAn\n", strlen("Vi du: abc: NguyenVanAn\n"), 0);
                    send(new_fd, "> ", 2, 0);
                }
            }
        }

        //  Dữ liệu từ các client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;
            if (fd == -1 || !FD_ISSET(fd, &read_fds)) continue;

            memset(buf, 0, BUF_SIZE);
            int n = recv(fd, buf, BUF_SIZE - 1, 0);

            if (n <= 0) {
                remove_client(i);
                continue;
            }

            trim_crlf(buf);

            // Trạng thái chờ đăng ký 
            if (clients[i].state == STATE_WAIT_ID) {
                // Tìm dấu ": " để tách id và name
                char *sep = strstr(buf, ": ");
                if (sep == NULL || sep == buf) {
                    send(fd, "Cu phap sai. Vui long gui: ID: ten\n> ", 37, 0);
                    continue;
                }

                // Tách id và name
                char id[64] = {0}, name[64] = {0};
                int id_len = sep - buf;
                if (id_len >= 64) id_len = 63;
                strncpy(id, buf, id_len);
                strncpy(name, sep + 2, 63);

                // Kiểm tra id và name không rỗng, name không chứa dấu cách
                if (strlen(id) == 0 || strlen(name) == 0) {
                    send(fd, "ID hoac ten khong duoc de trong.\n> ", 35, 0);
                    continue;
                }
                if (strchr(name, ' ') != NULL) {
                    send(fd, "Ten phai viet lien, khong chua dau cach.\n> ", 43, 0);
                    continue;
                }
                if (id_exists(id)) {
                    send(fd, "ID nay da ton tai, chon ID khac.\n> ", 35, 0);
                    continue;
                }

                // Đăng ký thành công
                strncpy(clients[i].id,   id,   63);
                strncpy(clients[i].name, name, 63);
                clients[i].state = STATE_CHAT;

                char welcome[BUF_SIZE];
                snprintf(welcome, BUF_SIZE,
                         "Xin chao %s (%s)! Ban da vao phong chat.\n",
                         name, id);
                send(fd, welcome, strlen(welcome), 0);

                // Thông báo cho phòng chat
                char ts[64]; get_timestamp(ts, sizeof(ts));
                char notify[BUF_SIZE];
                snprintf(notify, BUF_SIZE,
                         "%s [server]: %s (%s) da vao phong chat.\n",
                         ts, name, id);
                broadcast(notify, fd);

                printf("    [+] '%s' (%s) da dang ky thanh cong.\n", id, name);

            // Trạng thái chat 
            } else if (clients[i].state == STATE_CHAT) {
                if (strlen(buf) == 0) continue;

                char ts[64]; get_timestamp(ts, sizeof(ts));
                char msg[BUF_SIZE * 2];
                snprintf(msg, BUF_SIZE * 2, "%s %s: %s\n", ts, clients[i].id, buf);

                printf("    [msg] %s", msg);
                broadcast(msg, fd);
            }
        }
    }

    close(server_fd);
    return 0;
}