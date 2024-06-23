#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024
//işleyiciyi alma
void *receive_handler(void *socket_desc) {
    int sock = *(int *)socket_desc;
    char buffer[BUFFER_SIZE];
    int len;

    while ((len = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[len] = '\0';
        printf("%s", buffer);
        memset(buffer, 0, BUFFER_SIZE);
    }

    if (len == 0) {
        printf("Server disconnected\n");
    } else if (len == -1) {
        perror("recv failed");
    }

    return NULL;
}

int main(int argc, char **argv) {
    int sock;
    struct sockaddr_in server;
    pthread_t recv_thread;
    char message[BUFFER_SIZE];

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(1);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket");
        exit(1);
    }
    //server bağlantısı
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed");
        close(sock);
        exit(1);
    }

    printf("Connected to server\n");

    if (pthread_create(&recv_thread, NULL, receive_handler, (void *)&sock) < 0) {
        perror("could not create thread");
        close(sock);
        exit(1);
    }

    while (1) {
        fgets(message, BUFFER_SIZE, stdin);
        if (send(sock, message, strlen(message), 0) < 0) {
            perror("send failed");
            break;
        }
        memset(message, 0, BUFFER_SIZE);
    }

    close(sock);
    return 0;
}

