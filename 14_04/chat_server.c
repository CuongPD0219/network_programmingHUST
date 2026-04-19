#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define PORT        8080
#define MAX_CLIENTS 1000
#define BUF_SIZE    256

int main() {
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

    printf("Server is listening on port %d...\n", PORT);


    struct pollfd  fds[MAX_CLIENTS];
    char          *ids[MAX_CLIENTS];   // lưu trữ id của các client, chưa đăng nhập là NULL
    int nfds = 1;

    fds[0].fd     = listener;
    fds[0].events = POLLIN;
    memset(ids, 0, sizeof(ids));

    char buf[BUF_SIZE];

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) {
            perror("poll() failed");
            break;
        }

        // khi có client mới kết nối đến server
        if (fds[0].revents & POLLIN) {
            int client = accept(listener, NULL, NULL);

            if (client >= 0 && nfds < MAX_CLIENTS) {
                printf("New client connected: fd=%d\n", client);

                fds[nfds].fd     = client;
                fds[nfds].events = POLLIN;
                ids[nfds]        = NULL;    // chưa đăng nhập
                nfds++;

                char *msg = "Xin chao. Hay dang nhap voi cu phap: client_id: <ten>\n";
                send(client, msg, strlen(msg), 0);
            } else if (client >= 0) {
                close(client);
            }
        }

        // xử lý dữ liệu từ từng client
        for (int i = 1; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            ret = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);

            if (ret <= 0) {
                // client ngắt kết nối
                printf("Client %d ('%s') disconnected\n",
                       fds[i].fd, ids[i] ? ids[i] : "chua dang nhap");

                // thông báo cho các client khác nếu đã đăng nhập
                if (ids[i] != NULL) {
                    char notice[BUF_SIZE];
                    snprintf(notice, BUF_SIZE, "[Server] '%s' da roi phong chat.\n", ids[i]);
                    for (int j = 1; j < nfds; j++)
                        if (j != i && ids[j] != NULL)
                            send(fds[j].fd, notice, strlen(notice), 0);
                }

                close(fds[i].fd);
                free(ids[i]);

                // Xóa khỏi mảng: hoán đổi với phần tử cuối
                fds[i] = fds[nfds - 1];
                ids[i] = ids[nfds - 1];
                ids[nfds - 1] = NULL;
                nfds--;
                i--;  // kiểm tra lại vị trí vừa hoán vào
                continue;
            }

            buf[ret] = '\0';
            // Bỏ ký tự xuống dòng cuối
            int blen = strlen(buf);
            while (blen > 0 && (buf[blen-1] == '\n' || buf[blen-1] == '\r'))
                buf[--blen] = '\0';

            printf("Received from fd=%d: %s\n", fds[i].fd, buf);

            
            // kiểm tra cú pháp "client_id: <ten>"
            if (ids[i] == NULL) {
                char cmd[32], id[32], tmp[32];
                int n = sscanf(buf, "%31s %31s %31s", cmd, id, tmp);

                // chỉ có đúng 2 phần và phần đầu phải là "client_id:"
                if (n != 2) {
                    char *msg = "Error. Thua hoac thieu tham so! Cu phap: client_id: <ten>\n";
                    send(fds[i].fd, msg, strlen(msg), 0);
                    continue;
                }

                if (strcmp(cmd, "client_id:") != 0) {
                    char *msg = "Error. Sai cu phap! Cu phap: client_id: <ten>\n";
                    send(fds[i].fd, msg, strlen(msg), 0);
                    continue;
                }

                // đăng nhập thành công
                ids[i] = malloc(strlen(id) + 1);
                strcpy(ids[i], id);

                char ok[BUF_SIZE];
                snprintf(ok, BUF_SIZE, "Chao mung '%s'! Hay nhap tin nhan.\n"
                                       "  Gui cho tat ca:     all <tin_nhan_cua_ban>\n"
                                       "  Gui rieng cho <id>: <id> <tin_nhan_cua_ban>\n", id);
                send(fds[i].fd, ok, strlen(ok), 0);

                // Thông báo cho các client khác
                char notice[BUF_SIZE];
                snprintf(notice, BUF_SIZE, "Server: '%s' vua vao phong chat.\n", id);
                for (int j = 1; j < nfds; j++)
                    if (j != i && ids[j] != NULL)
                        send(fds[j].fd, notice, strlen(notice), 0);

                printf("    Dang ky: id='%s' fd=%d\n", id, fds[i].fd);

            // đăng nhập thành công và chuyển tiếp tin nhắn
            } else {
                char target[32], content[BUF_SIZE];

                // tách phần đầu (target) và phần còn lại (nội dung)
                int n = sscanf(buf, "%31s", target);
                if (n == 0) continue;

                // phần nội dung nằm sau target
                char *pos = buf + strlen(target);
                while (*pos == ' ') pos++;  // bỏ khoảng trắng giữa

                if (strlen(pos) == 0) {
                    char *msg = "Error. Thieu noi dung tin nhan!\n";
                    send(fds[i].fd, msg, strlen(msg), 0);
                    continue;
                }

                snprintf(content, BUF_SIZE, "%s: %s\n", ids[i], pos);

                if (strcmp(target, "all") == 0) {
                    // gửi cho tất cả client đã đăng nhập (trừ người gửi)
                    for (int j = 1; j < nfds; j++)
                        if (j != i && ids[j] != NULL)
                            send(fds[j].fd, content, strlen(content), 0);

                } else {
                    // tìm client theo id
                    int j = 1;
                    for (; j < nfds; j++)
                        if (ids[j] != NULL && strcmp(target, ids[j]) == 0)
                            break;

                    if (j < nfds) {
                        send(fds[j].fd, content, strlen(content), 0);
                    } else {
                        char err[BUF_SIZE];
                        snprintf(err, BUF_SIZE,
                                 "Error. Khong tim thay client '%s'.\n", target);
                        send(fds[i].fd, err, strlen(err), 0);
                    }
                }
            }
        }
    }

    for (int i = 1; i < nfds; i++) {
        free(ids[i]);
        close(fds[i].fd);
    }
    close(listener);
    return 0;
}