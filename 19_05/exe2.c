#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>

#define PORT 8090
#define BUFFER_SIZE 1024

// Lưu trữ cặp socket để truyền vào luồng
typedef struct {
    int client1;
    int client2;
} ClientPair;

// Biến toàn cục để lưu socket của client đang chờ ghép cặp (-1 nghĩa là hàng đợi trống)
int waiting_client_sock = -1;
// Mutex để đảm bảo an toàn khi nhiều luồng cùng truy cập hàng đợi
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// Hàm xử lý một phiên chat giữa 2 client
void *chat_session(void *arg);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Tạo socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Tùy chọn để tái sử dụng port ngay lập tức (tránh lỗi "Address already in use" khi khởi động lại server)
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind() failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listen() failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Chat server dang lang nghe tren port %d...\n", PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Co client moi ket noi (Socket FD: %d)\n", client_sock);

        // Khóa Mutex trước khi thao tác với biến toàn cục waiting_client_sock
        pthread_mutex_lock(&queue_mutex);

        if (waiting_client_sock == -1) {
            // Hàng đợi đang trống, lưu client này vào chờ
            waiting_client_sock = client_sock;
            char *wait_msg = "SERVER: Ban da ket noi. Dang cho ghep cap voi client khac...\r\n";
            send(client_sock, wait_msg, strlen(wait_msg), 0);
            
            pthread_mutex_unlock(&queue_mutex);
        } else {
            // Đã có 1 client đang chờ -> Ghép cặp 2 client này
            ClientPair *pair = malloc(sizeof(ClientPair));
            pair->client1 = waiting_client_sock;
            pair->client2 = client_sock;
            
            // Xóa rỗng hàng đợi để đón cặp tiếp theo
            waiting_client_sock = -1;
            pthread_mutex_unlock(&queue_mutex);

            // Tạo luồng mới để quản lý riêng phiên chat của cặp này
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, chat_session, (void *)pair) < 0) {
                perror("Khong the tao luong phien chat");
                free(pair);
                printf("[Client FD: %d] dong ket noi\n", pair->client1);
                printf("[Client FD: %d] dong ket noi\n", pair->client2);
                close(pair->client1);
                close(pair->client2);
                continue;
            }
            
            // Tách luồng để hệ thống tự thu hồi tài nguyên khi kết thúc chat
            pthread_detach(thread_id);
            printf("Da ghep cap socket %d va %d thanh cong.\n", pair->client1, pair->client2);
        }
    }

    close(server_sock);
    return 0;
}

void *chat_session(void *arg) {
    ClientPair *pair = (ClientPair *)arg;
    int sock1 = pair->client1;
    int sock2 = pair->client2;
    free(pair); // Giải phóng bộ nhớ đã cấp phát ở main()

    char buffer[BUFFER_SIZE];
    char msg[BUFFER_SIZE+64];
    fd_set readfds;
    int max_sd = (sock1 > sock2) ? sock1 : sock2;

    // Thông báo cho cả 2 client biết đã được ghép cặp thành công
    char matched_msg1[100], matched_msg2[100] ;
    sprintf(matched_msg1, "SERVER: Da ghep cap thanh cong! Ban co the bat dau chat voi [Client FD: %d].\r\n", sock2);
    sprintf(matched_msg2, "SERVER: Da ghep cap thanh cong! Ban co the bat dau chat voi [Client FD: %d].\r\n", sock1);
    
    send(sock1, matched_msg2, strlen(matched_msg2), 0);
    send(sock2, matched_msg1, strlen(matched_msg1), 0);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock1, &readfds);
        FD_SET(sock2, &readfds);

        // dùng select() để chờ dữ liệu từ một trong hai client 
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Loi select()");
            break;
        }

        // Nếu có tin nhắn từ Client 1
        if (FD_ISSET(sock1, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int valread = recv(sock1, buffer, BUFFER_SIZE - 1, 0);
            if (valread <= 0) {
                break; // Client 1 ngắt kết nối
            }
            // Chuyển tiếp sang Client 2
            memset(msg, 0, sizeof(msg));
            sprintf(msg, "[Client FD: %d]: %s", sock1, buffer);
            printf("%s", msg);
            send(sock2, msg,sizeof(msg), 0);
        }

        // Nếu có tin nhắn từ Client 2
        if (FD_ISSET(sock2, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int valread = recv(sock2, buffer, BUFFER_SIZE - 1, 0);
            if (valread <= 0) {
                break; // Client 2 ngắt kết nối
            }
            // Chuyển tiếp sang Client 1
            memset(msg, 0, sizeof(msg));
            sprintf(msg, "[Client FD: %d]: %s", sock2, buffer);
            printf("%s", msg);
            send(sock1, msg, sizeof(msg), 0);
        }
    }

    // Nếu vòng lặp kết thúc (1 trong 2 client ngắt kết nối)
    printf("Mot client da ngat ket noi. Dong phien chat giua %d va %d.\n", sock1, sock2);
    char *dc_msg = "SERVER: Doi phuong da ngat ket noi. Tam biet!\r\n";
    
    // Sử dụng MSG_NOSIGNAL để tránh Server bị crash (nhận tín hiệu SIGPIPE) nếu socket đã đóng
    send(sock1, dc_msg, strlen(dc_msg), MSG_NOSIGNAL);
    send(sock2, dc_msg, strlen(dc_msg), MSG_NOSIGNAL);

    // Ngắt kết nối của cả 2 client
    close(sock1);
    close(sock2);
    pthread_exit(NULL);
}