#include<stdio.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<poll.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>

#define MAX_CLIENTS 100
#define PORT 9001
#define MAX_BUFFERS 2048

typedef struct{
    int fd;
    char topics[10][50];
    int topic_count;
}ClientsInfo;

//# Kiem tra client co dang ky nhan tin topic nay khong
int checkTopic(const ClientsInfo c, char topicClient[]){
    int topicCount = c.topic_count;
    int j = 0;
    for(;j < topicCount ; j++){
        if(strcmp(c.topics[j], topicClient) ==0){
            return j;
        }
    }
    if(j == topicCount)
        return j;
}


int main(){
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(listener < 0){
        perror("socket() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if(bind(listener, (struct sockaddr *)&addr, sizeof(addr))){
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if(listen(listener, MAX_CLIENTS)){
         perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server is listening on port %d...\n", PORT);

    int clientCount = 0;
   
    ClientsInfo clientList[MAX_CLIENTS];
    struct pollfd fds[MAX_CLIENTS];
    char buf[MAX_BUFFERS];
    char notification[MAX_BUFFERS];
    char info[1024], introduction[1024];
    int nfds = 1;

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    while(1){
        int ret = poll(fds, nfds, -1);

        if(ret <= 0){
            perror("poll() failed.\n");
            break;
        }

        
        if(fds[0].revents & POLLIN){
            int client = accept(listener, NULL, NULL);

            if(nfds < MAX_CLIENTS){
                // Thong bao cho server va client khac biet
                sprintf(info,  "\nClient co fd = %d da tham gia phong chat!\n", client);
                printf("%s\n", info);
                for(int c = 1; c < nfds ; c++){
                    send(fds[c].fd, info, strlen(info), 0);
                }

                // Gui loi chao den client
                memset(info, 0 , sizeof(info));
                snprintf(info, sizeof(info) -1, "Chao mung ban den voi server!\nGo exit de thoat chuong trinh.\n");
                snprintf(introduction, sizeof(info) -1, "\tSUB <topic>: dang ky chat voi topic.\n\tPUB <topic> <message> : gui tin nhan voi topic.\n\tUNSUB <topic> : huy dang ky topic.\n");
                send(client, info, strlen(info), 0);
                send(client, introduction, strlen(introduction), 0);

                clientList[clientCount].fd = client;
                fds[nfds].fd = client;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                nfds++; clientCount++;
            }else{
                close(client);
            }
            
        }

        // Kiem tra trang thai cua tung client
        for(int i = 1; i < nfds ; i++){
            if(fds[i].revents & POLLIN){ // client gui tin nhan den
                // Luu tin nhan nhan duoc tu client
                memset(buf, 0, MAX_BUFFERS);
                int receiveData= recv(fds[i].fd, buf, sizeof(buf)-1, 0);
                buf[strcspn(buf, "\r\n")] = 0;

                // Client gui thong bao ngat ket noi
                if(receiveData <= 0 || strcmp(buf, "exit") ==0){
                    // Thong bao cho server va cac client khac biet
                    char notice[MAX_BUFFERS] ;
                    snprintf(notice, MAX_BUFFERS, "Client co fd = %d da roi khoi phong chat!\n", fds[i].fd);
                    printf("%s", notice);
                    for(int j = 1 ; j < nfds ; j++){
                        if(j != i ){
                            send(fds[j].fd, notice, strlen(notice), 0);
                        }
                    }

                    // Ngat ket noi va dieu chinh lai cac tham so
                    for(int c = 0 ; c < clientCount ; c++){
                        if(fds[i].fd == clientList[c].fd){
                            if(clientCount == 1 || c == clientCount- 1){
                                memset(&clientList[c], 0 , sizeof(ClientsInfo));
                            }
                            else{
                                clientList[c] = clientList[clientCount-1];
                                memset(&clientList[clientCount - 1], 0 , sizeof(ClientsInfo));
                            }
                            clientCount--;
                            break;
                        }
                    }
                    close(fds[i].fd);
                    fds[i]= fds[nfds - 1];
                    nfds --;
                    i--;
                    continue;
                }

                // Client gui tin nhan thong thuong
                else{
                    // tim client hien tai
                    int clientCurrent = 0;
                    for(; clientCurrent < clientCount ; clientCurrent++){
                        if(clientList[clientCurrent].fd == fds[i].fd)
                            break;
                    }
                    int clientTopicCount = clientList[clientCurrent].topic_count;

                    memset(notification,0, sizeof(notification));

                    // truyen du lieu nhan duoc sang cac bien can thiet
                    char request[10],  topicMsg[100] , msg[1500];
                    int checking = sscanf(buf, "%s %s %[^\n]", request, topicMsg , msg);

                    // kiem tra dau vao dinh dang
                    if(checking == 2){
                        // Client dang ky topic
                        if(strcmp(request, "SUB") ==0){
                            if(clientTopicCount == 0 || checkTopic(clientList[clientCurrent], topicMsg) == clientTopicCount){
                                strcpy(clientList[clientCurrent].topics[clientTopicCount], topicMsg);
                                snprintf(notification, MAX_BUFFERS -1, "Client co [fd = %d] dang ky nhan tin trong [TOPIC: %s] thanh cong!\n", fds[i].fd, topicMsg);
                                printf("%s",notification);
                                send(fds[i].fd, notification, strlen(notification), 0);
                                clientList[clientCurrent].topic_count++;
                            }else{
                                snprintf(notification, MAX_BUFFERS-1, "Ban da dang ky topic [%s] thanh cong! Vui long khong gui dang ky lai!\n",topicMsg);
                                send(fds[i].fd, notification, strlen(notification), 0);
                            }
                        }

                         // Client huy dang ky topic
                        else if (strcmp(request, "UNSUB") ==0){
                            int pos = checkTopic(clientList[clientCurrent], topicMsg);
                            if(clientTopicCount ==0 || pos == clientTopicCount){
                                snprintf(notification, MAX_BUFFERS -1, "Ban chua dang ky topic nay, khong the huy dang ky!\n");
                                send(fds[i].fd, notification, strlen(notification), 0);
                            }else{
                                if((clientTopicCount != 1) && pos != clientTopicCount -1)
                                    strcpy(clientList[clientCurrent].topics[pos], clientList[clientCurrent].topics[clientTopicCount-1]);
                                clientList[clientCurrent].topic_count--;
                                snprintf(notification, MAX_BUFFERS-1, "Client co [fd = %d] da huy dang ky [TOPIC: %s] thanh cong\n",fds[i].fd, topicMsg);
                                printf("%s", notification);
                                for(int f = 0; f < clientCount ; f++){
                                    if(clientList[f].fd != fds[i].fd && (checkTopic(clientList[f], topicMsg) != clientList[f].topic_count)){
                                        send(clientList[f].fd, notification, strlen(notification), 0);
                                    }
                                }
                            }
                        }

                        // Sai dau vao dinh dang
                        else{
                            snprintf(notification, MAX_BUFFERS, "Ban da gui sai dinh dang!.Vui long gui lai dung dinh dang tin nhan sau:\n");
                            send(fds[i].fd, notification, strlen(notification),0);
                            send(fds[i].fd, introduction, strlen(introduction), 0);
                        }
                    }
                    else{
                        // Client gui tin nhan trong topic
                        if(checking ==3 && strcmp(request, "PUB") ==0){
                            if(clientTopicCount ==0 || checkTopic(clientList[clientCurrent], topicMsg) == clientTopicCount){
                                snprintf(notification, MAX_BUFFERS -1, "Ban chua  dang ky topic nay, vui long dang ky truoc khi nhan tin!\n");
                                send(fds[i].fd, notification, strlen(notification), 0);
                            }else{
                                snprintf(notification, MAX_BUFFERS-1, "[fd = %d] [TOPIC: %s]: %s\n",fds[i].fd, topicMsg, msg);
                                    printf("%s", notification);
                                    for(int f = 0; f < clientCount ; f++){
                                        if(clientList[f].fd != fds[i].fd && (checkTopic(clientList[f], topicMsg) != clientList[f].topic_count)){
                                            send(clientList[f].fd, notification, strlen(notification), 0);
                                        }
                                    }
                            }
                        }

                        // Sai dau vao dinh dang
                        else{
                            snprintf(notification, MAX_BUFFERS, "Ban da gui sai dinh dang!.Vui long gui lai dung dinh dang tin nhan sau:\n");
                            send(fds[i].fd, notification, strlen(notification),0);
                            send(fds[i].fd, introduction, strlen(introduction), 0);
                        }
                    }
                    
                }

            }
        }


    }

    for (int i = 1; i < nfds; i++) {
        close(fds[i].fd);
    }


    close(listener);



}