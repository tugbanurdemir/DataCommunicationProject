// client_windows.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <pthread.h>
#include <signal.h>

#pragma comment(lib,"ws2_32.lib") // Winsock kütüphanesini linkle

#define PORT 8080
#define BUFFER_SIZE 2048

volatile sig_atomic_t flag = 0;
SOCKET sockfd = 0;
char name[32];

// String sonundaki boşlukları kaldırma
void strtrim(char* arr, int length){
    int i;
    for(i = 0; i < length; i++){
        if(arr[i] == '\n'){
            arr[i] = '\0';
            break;
        }
    }
}

// Çıkış sinyali işleyicisi
void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

// Mesaj gönderme
void send_msg_handler() {
    char message[BUFFER_SIZE] = {};
    char buffer[BUFFER_SIZE + 32] = {};

    while(1){
        fgets(message, BUFFER_SIZE, stdin);
        strtrim(message, BUFFER_SIZE);

        if(strcmp(message, "çıkış") == 0){
            break;
        } else {
            sprintf(buffer, "%s: %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
        }

        memset(message, 0, BUFFER_SIZE);
        memset(buffer, 0, BUFFER_SIZE + 32);
    }
    catch_ctrl_c_and_exit(2);
}

// Mesaj alma
void recv_msg_handler() {
    char message[BUFFER_SIZE] = {};
    while(1){
        int receive = recv(sockfd, message, BUFFER_SIZE, 0);
        if(receive > 0){
            printf("%s", message);
            fflush(stdout);
        } else if(receive == 0){
            break;
        } else {
            // Hata
        }
        memset(message, 0, sizeof(message));
    }
}

int main(){
    signal(SIGINT, catch_ctrl_c_and_exit);

    printf("İsminizi girin: ");
    fgets(name, 32, stdin);
    strtrim(name, strlen(name));

    if(strlen(name) > 31 || strlen(name) < 2){
        printf("İsim 2 ile 31 karakter arasında olmalıdır.\n");
        return EXIT_FAILURE;
    }

    // Winsock başlatma
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0){
        printf("Hata: Winsock başlatılamadı. Error Code : %d",WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Soket oluşturma
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == INVALID_SOCKET){
        printf("Hata: Soket oluşturulamadı. Error Code : %d", WSAGetLastError());
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Sunucu adresi
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0){
        printf("Geçersiz adres.\n");
        return EXIT_FAILURE;
    }

    // Bağlanma
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        printf("Bağlantı başarısız. Error Code : %d", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // İsim gönderme
    send(sockfd, name, 32, 0);

    printf("=== SOHBETE HOŞ GELDİNİZ ===\n");

    pthread_t send_thread;
    if(pthread_create(&send_thread, NULL, (void *) send_msg_handler, NULL) != 0){
        printf("Hata: pthread_create\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_thread;
    if(pthread_create(&recv_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
        printf("Hata: pthread_create\n");
        return EXIT_FAILURE;
    }

    while(1){
        if(flag){
            printf("\nÇıkış yapılıyor...\n");
            break;
        }
    }

    closesocket(sockfd);
    WSACleanup();

    return EXIT_SUCCESS;
}
