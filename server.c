// server_windows.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <pthread.h>

#pragma comment(lib,"ws2_32.lib") // Winsock kütüphanesini linkle

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048

static unsigned int client_count = 0;
static int uid = 10;

// İstemci yapısı
typedef struct {
    struct sockaddr_in address;
    SOCKET sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// İstemci ekleme
void add_client(client_t *cl){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i < MAX_CLIENTS; ++i){
        if(!clients[i]){
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// İstemci çıkarma
void remove_client(int uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i < MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid == uid){
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Mesaj gönderme
void send_message(char *s, int uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid != uid){
                if(send(clients[i]->sockfd, s, strlen(s), 0) < 0){
                    perror("Hata: Mesaj gönderilemedi");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// Mesaj alımı ve gönderimi
void *handle_client(void *arg){
    char buff_out[BUFFER_SIZE];
    char name[32];
    int leave_flag = 0;

    client_count++;
    client_t *cli = (client_t *)arg;

    // İsim alımı
    if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 31){
        printf("Giriş ismi geçersiz.\n");
        leave_flag = 1;
    } else {
        strcpy(cli->name, name);
        sprintf(buff_out, "%s sohbete katıldı.\n", cli->name);
        printf("%s", buff_out);
        send_message(buff_out, cli->uid);
    }

    memset(buff_out, 0, BUFFER_SIZE);

    while(1){
        if (leave_flag) {
            break;
        }

        int receive = recv(cli->sockfd, buff_out, BUFFER_SIZE, 0);
        if (receive > 0){
            if(strlen(buff_out) > 0){
                send_message(buff_out, cli->uid);
                printf("%s", buff_out);
            }
        } else if (receive == 0 || strcmp(buff_out, "çıkış") == 0){
            sprintf(buff_out, "%s sohbetten ayrıldı.\n", cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli->uid);
            leave_flag = 1;
        } else {
            printf("Hata: -1\n");
            leave_flag = 1;
        }

        memset(buff_out, 0, BUFFER_SIZE);
    }

    // Bağlantı sonlandırma
    closesocket(cli->sockfd);
    remove_client(cli->uid);
    free(cli);
    client_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(){
    int option = 1;
    SOCKET listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // Winsock başlatma
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0){
        printf("Hata: Winsock başlatılamadı. Error Code : %d",WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Soket oluşturma
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == INVALID_SOCKET){
        printf("Hata: Soket oluşturulamadı. Error Code : %d", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Soket seçenekleri
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0){
        printf("Hata: setsockopt başarısız. Error Code : %d", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Sunucu adresi ayarı
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Bağlama
    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR){
        printf("Hata: Bağlama başarısız. Error Code : %d", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Dinleme
    if(listen(listenfd, 10) == SOCKET_ERROR){
        printf("Hata: Dinleme başarısız. Error Code : %d", WSAGetLastError());
        return EXIT_FAILURE;
    }

    printf("=== SOHBET SUNUCUSU BAŞLATILDI ===\n");

    while(1){
        int clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        if(connfd == INVALID_SOCKET){
            printf("Hata: Bağlantı kabul edilemedi. Error Code : %d", WSAGetLastError());
            continue;
        }

        // Maksimum istemci kontrolü
        if((client_count + 1) == MAX_CLIENTS){
            printf("Maksimum istemci sayısına ulaşıldı. Bağlantı reddedildi.\n");
            closesocket(connfd);
            continue;
        }

        // İstemci yapısı oluşturma
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        // İstemciyi ekleme ve iş parçacığı başlatma
        add_client(cli);
        pthread_create(&tid, NULL, &handle_client, (void*)cli);

        Sleep(1);
    }

    // Winsock temizleme
    WSACleanup();

    return EXIT_SUCCESS;
}
