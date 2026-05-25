// Chat server : multithread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define PORT        8080
#define MAX_CLIENTS 1000
#define BUF_SIZE    256

// Cấu trúc lưu trữ thông tin client
typedef struct {
    int fd;         // File descriptor của client (nếu = 0 nghĩa là slot trống)
    char id[32];    // Tên đăng nhập
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];

// Mutex để bảo vệ việc truy cập mảng clients dùng chung giữa các luồng
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Hàm xử lý riêng cho từng client
void *client_handler(void *arg) ;

int main() {
    // Khởi tạo mảng clients trống (fd = 0)
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        memset(clients[i].id, 0, sizeof(clients[i].id));
    }

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server is listening on port %d... \n", PORT);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            perror("accept() failed");
            continue;
        }

        printf("New connection accepted: fd=%d\n", client);

        int *client_sock = malloc(sizeof(int));
        *client_sock = client;

        pthread_t tid;
        // Tạo một luồng mới cho client
        if (pthread_create(&tid, NULL, client_handler, (void*)client_sock) != 0) {
            perror("pthread_create() failed");
            close(client);
            free(client_sock);
        }

        // Tách luồng để hệ thống tự thu hồi tài nguyên sau khi luồng kết thúc (không cần dùng pthread_join)
        pthread_detach(tid);
    }

    close(listener);
    return 0;
}

void *client_handler(void *arg) {
    // Lấy fd từ con trỏ arg và giải phóng bộ nhớ đã cấp phát
    int client_fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    char id[32] = {0};
    int is_logged_in = 0;

    // Gửi lời chào khi mới kết nối
    char *welcome_msg = "Xin chao. Hay dang nhap voi cu phap: client_id: <ten>\n";
    send(client_fd, welcome_msg, strlen(welcome_msg), 0);

    while (1) {
        int ret = recv(client_fd, buf, sizeof(buf) - 1, 0);
        
        // Nhận <= 0 nghĩa là client ngắt kết nối hoặc có lỗi
        if (ret <= 0) {
            break; 
        }

        buf[ret] = '\0';

        // Xóa ký tự xuống dòng ở cuối (nếu có)
        int blen = strlen(buf);
        while (blen > 0 && (buf[blen-1] == '\n' || buf[blen-1] == '\r')) {
            buf[--blen] = '\0';
        }
        if (blen == 0) continue;

        printf("Received from fd=%d: %s\n", client_fd, buf);

        // xử lý đăng nhập
        if (!is_logged_in) {
            char cmd[32], tmp_id[32], tmp[32];
            int n = sscanf(buf, "%31s %31s %31s", cmd, tmp_id, tmp);

            if (n != 2 || strcmp(cmd, "client_id:") != 0) {
                char *msg = "Error. Sai cu phap hoac thua/thieu tham so! Cu phap: client_id: <ten>\n";
                send(client_fd, msg, strlen(msg), 0);
                continue;
            }

            // Đăng nhập thành công
            strcpy(id, tmp_id);
            is_logged_in = 1;

            // Đưa client vào danh sách quản lý chung 
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) { // Tìm vị trí trống
                    clients[i].fd = client_fd;
                    strcpy(clients[i].id, id);
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            // Gửi hướng dẫn
            char ok[BUF_SIZE];
            snprintf(ok, BUF_SIZE, "Chao mung '%s'! Hay nhap tin nhan.\n"
                                   "  Gui cho tat ca:     all <tin_nhan_cua_ban>\n"
                                   "  Gui rieng cho <id>: <id> <tin_nhan_cua_ban>\n", id);
            send(client_fd, ok, strlen(ok), 0);

            // Thông báo cho các client khác (Cần lock mutex)
            char notice[BUF_SIZE];
            snprintf(notice, BUF_SIZE, "Server: Client co id = '%s' vua vao phong chat.\n", id);
            
            pthread_mutex_lock(&clients_mutex);
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (clients[j].fd != 0 && clients[j].fd != client_fd) {
                    send(clients[j].fd, notice, strlen(notice), 0);
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            printf("\tDang ky: id='%s' fd=%d\n", id, client_fd);

        } 

        // Xử lý khi nhận tin nhắn
        else {
            char target[32], content[BUF_SIZE];
            int n = sscanf(buf, "%31s", target);
            if (n == 0) continue;

            // Tách nội dung tin nhắn
            char *pos = buf + strlen(target);
            while (*pos == ' ') pos++;  // Bỏ qua khoảng trắng

            if (strlen(pos) == 0) {
                char *msg = "Error. Thieu noi dung tin nhan!\n";
                send(client_fd, msg, strlen(msg), 0);
                continue;
            }

            snprintf(content, BUF_SIZE, "[Client_id = %s]: %s\n", id, pos);

            // Broadcast và Unicast (Cần lock mutex vì duyệt mảng clients)
            pthread_mutex_lock(&clients_mutex);
            
            if (strcmp(target, "all") == 0) {
                // Gửi cho tất cả mọi người trừ người gửi
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].fd != 0 && clients[j].fd != client_fd) {
                        send(clients[j].fd, content, strlen(content), 0);
                    }
                }
            } else {
                // Tìm kiếm id cụ thể
                int found = 0;
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].fd != 0 && strcmp(clients[j].id, target) == 0) {
                        send(clients[j].fd, content, strlen(content), 0);
                        found = 1;
                        break;
                    }
                }
                
                if (!found) {
                    char err[BUF_SIZE];
                    snprintf(err, BUF_SIZE, "Error. Khong tim thay client '%s'.\n", target);
                    send(client_fd, err, strlen(err), 0);
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }
    }

    // Ngắt kết nối
    if (is_logged_in) {
        printf("Client fd=%d ('%s') disconnected\n", client_fd, id);
        
        char notice[BUF_SIZE];
        snprintf(notice, BUF_SIZE, "[Server]: Client có id = %s da roi phong chat.\n", id);

        // Lock mutex để xóa thông tin client và gửi thông báo rời đi
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == client_fd) {
                clients[i].fd = 0; // Đánh dấu slot trống
                memset(clients[i].id, 0, sizeof(clients[i].id));
            } else if (clients[i].fd != 0) {
                send(clients[i].fd, notice, strlen(notice), 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    } else {
        printf("Client fd=%d (chua dang nhap) disconnected\n", client_fd);
    }

    close(client_fd);
    pthread_exit(NULL);
}