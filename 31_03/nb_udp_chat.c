#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/select.h>

#define MAX_BUF 1024

int main(int argc, char *argv[]){
    if (argc != 4) {
        fprintf(stderr, "Usage: %s port_s ip_d port_d\n", argv[0]);
        fprintf(stderr, "  port_s : cong lang nghe cua ung dung nay\n");
        fprintf(stderr, "  ip_d   : IP cua ung dung dich\n");
        fprintf(stderr, "  port_d : cong cua ung dung dich\n");
        return 1;
    }

    //tham so dau vao
    int port_s = atoi(argv[1]);
    char *ip_d = argv[2];
    int port_d = atoi(argv[3]);

    // Tao UDP socket
    int sock_UDP = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock_UDP < 0){
        perror("Socket");
        return 1;
    }

    //Bind
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port_s);
    if(bind(sock_UDP, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0){
        perror("Bind");
        return 1;
    }

    // Thiet lap dia chi dich
    struct sockaddr_in des_addr;
    memset(&des_addr, 0, sizeof(des_addr));
    des_addr.sin_family = AF_INET;
    des_addr.sin_port = htons(port_d);
    if(inet_pton(AF_INET, ip_d, &des_addr.sin_addr) <= 0){
        fprintf(stderr, "IP khong hop le: %s\n", ip_d);
        return 1;
    }

    printf("UDP CHAT APP\n");
    printf("Lang nghe   : 0.0.0.0:%d\n", port_s);
    printf("Gui den     : %s:%d\n", ip_d, port_d);
    printf("Go 'quit' de thoat ung dung.\n\n");

    char str[MAX_BUF];
    fd_set read_fds;

    while(1){
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); // theo doi dau vao
        FD_SET(sock_UDP, &read_fds); // theo doi socket UDP

        int max_fd = sock_UDP; 
        int activity = select(max_fd +1, &read_fds , NULL, NULL, NULL);
        if(activity < 0){
            perror("Select");
            break;
        }

        // Nhan du lieu tu noi khac
        if(FD_ISSET(sock_UDP, &read_fds)){
            struct sockaddr_in sender;
            socklen_t sender_len = sizeof(sender);
            memset(str, 0, MAX_BUF);

            int n = recvfrom(sock_UDP, str, MAX_BUF -1 , 0 , (struct sockaddr*)&sender, &sender_len);
            if(n > 0 ){
                printf("\r[%s:%d] %s\nBan: ", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port), str);
                fflush(stdout);
            }
        }

        // Nguoi dung nhap du lieu tu ban phim
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(str, 0, MAX_BUF);
            if (fgets(str, MAX_BUF, stdin) == NULL) break;
 
            // xoa dau xuong dong
            str[strcspn(str, "\n")] = 0;
 
            if (strcmp(str, "quit") == 0) break;
 
            if (strlen(str) == 0) {
                printf("Ban: ");
                fflush(stdout);
                continue;
            }
 
            sendto(sock_UDP, str, strlen(str), 0,
                   (struct sockaddr*)&des_addr, sizeof(des_addr));
 
            printf("Ban: ");
            fflush(stdout);
        }
    }

    printf("\nDa thoat. Ket thuc ket noi! \n");
    close(sock_UDP);
    return 0;



    
    




}