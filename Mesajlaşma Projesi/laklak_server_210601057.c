#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define USER_FILE "users.txt"

// İstemci yapısı
typedef struct {
    int socket;                        
    struct sockaddr_in address;         
    char username[50];                 
    int logged_in;                      
} client_t;

client_t *clients[MAX_CLIENTS];        
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;  

// İstemci ekleme fonksiyonu
void add_client(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == NULL) {       
            clients[i] = cl;           
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// İstemci kaldırma fonksiyonu
void remove_client(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == cl) {         // Belirtilen istemciyi bulursa
            clients[i] = NULL;          // İstemciyi kaldırır
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Genel mesaj gönderme fonksiyonu
void broadcast_message(char *message, client_t *sender) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->logged_in && clients[i] != sender) {
            // Gönderici dışındaki tüm bağlı istemcilere mesaj gönderir
            send(clients[i]->socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Özel mesaj gönderme fonksiyonu
void send_private_message(char *message, char *receiver_username, client_t *sender) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->logged_in && strcmp(clients[i]->username, receiver_username) == 0) {
            // Belirtilen kullanıcı adına sahip istemciye özel mesaj gönderir
            send(clients[i]->socket, message, strlen(message), 0);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Kullanıcı kimlik bilgilerini kontrol etme fonksiyonu
int check_user_credentials(char *username, char *password) {
    FILE *file = fopen(USER_FILE, "r");
    if (!file) return 0;

    char line[256];
    char stored_username[50], stored_password[50];
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s %s", stored_username, stored_password);
        if (strcmp(username, stored_username) == 0 && strcmp(password, stored_password) == 0) {
            // Kullanıcı adı ve parola eşleşirse
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

// Yeni kullanıcı kaydı fonksiyonu
void register_user(char *username, char *password, char *fullname) {
    FILE *file = fopen(USER_FILE, "a");
    if (file) {
        fprintf(file, "%s %s %s\n", username, password, fullname);
        fclose(file);
    }
}

// İstemci işlemlerini yönetme fonksiyonu
void *handle_client(void *arg) {
    char buffer[BUFFER_SIZE];
    int leave_flag = 0;
    client_t *cli = (client_t *)arg;

    while (!leave_flag) {
        int receive = recv(cli->socket, buffer, BUFFER_SIZE, 0);
        if (receive > 0) {
            buffer[receive] = '\0';
            char command[10];
            sscanf(buffer, "%s", command);

            // Komutlar işleniyor
            if (strcmp(command, "REGISTER") == 0) {
                char username[50], password[50], fullname[100];
                sscanf(buffer, "%*s %s %s %s", username, password, fullname);
                register_user(username, password, fullname);
                snprintf(buffer, BUFFER_SIZE, "User %s registered successfully.\n", username);
                send(cli->socket, buffer, strlen(buffer), 0);
            } else if (strcmp(command, "LOGIN") == 0) {
                char username[50], password[50];
                sscanf(buffer, "%*s %s %s", username, password);
                if (check_user_credentials(username, password)) {
                    strcpy(cli->username, username);
                    cli->logged_in = 1;
                    snprintf(buffer, BUFFER_SIZE, "User %s logged in successfully.\n", username);
                } else {
                    snprintf(buffer, BUFFER_SIZE, "Invalid username or password.\n");
                }
                send(cli->socket, buffer, strlen(buffer), 0);
            } else if (strcmp(command, "LIST") == 0) {
                // TODO: LIST komutu implemente edilecek
            } else if (strcmp(command, "LOGOUT") == 0) {
                cli->logged_in = 0;
                snprintf(buffer, BUFFER_SIZE, "User %s logged out.\n", cli->username);
                send(cli->socket, buffer, strlen(buffer), 0);
                leave_flag = 1;
            } else if (strcmp(command, "MSG") == 0) {
                char receiver[50], message[BUFFER_SIZE];
                sscanf(buffer, "%*s %s %[^\n]", receiver, message);
                char full_message[BUFFER_SIZE];
                snprintf(full_message, BUFFER_SIZE, "%s: %s\n", cli->username, message);
                if (strcmp(receiver, "*") == 0) {
                    broadcast_message(full_message, cli); // Genel mesaj
                } else {
                    send_private_message(full_message, receiver, cli); // Özel mesaj
                }
            } else if (strcmp(command, "INFO") == 0) {
                // TODO: INFO komutu implemente edilecek
            }
        } else if (receive == 0 || strcmp(buffer, "exit") == 0) {
            leave_flag = 1; // İstemci bağlantıyı kapattı
        } else {
            perror("recv");
            leave_flag = 1; // Alınan veri hatalı
        }

        memset(buffer, 0, BUFFER_SIZE); // Buffer sıfırlanıyor
    }

    close(cli->socket); // İstemci soketi kapatılıyor
    remove_client(cli); // İstemci listeden çıkarılıyor
    free(cli); // Bellek serbest bırakılıyor
    pthread_detach(pthread_self()); // Thread ayrılıyor

    return NULL;
}

int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr, new_addr;
    socklen_t addr_size;
    pthread_t tid;

    // Sunucu soketi oluşturuluyor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4444);

    // Soket adres ile bağlanıyor
    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    // Soket dinlemeye başlıyor
    listen(server_socket, 10);

    printf("Server started on port 4444\n");

    while (1) {
        addr_size = sizeof(new_addr);
        new_socket = accept(server_socket, (struct sockaddr *)&new_addr, &addr_size);

        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        // Yeni istemci oluşturuluyor
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = new_addr;
        cli->socket = new_socket;
        cli->logged_in = 0;

        if (clients_count < MAX_CLIENTS) {
            add_client(cli); // İstemci ekleniyor
            pthread_create(&tid, NULL, handle_client, (void *)cli); // Yeni thread oluşturuluyor
        } else {
            // Maksimum istemci sayısına ulaşıldı
            printf("Max clients reached. Rejected: %s:%d\n", inet_ntoa(new_addr.sin_addr), ntohs(new_addr.sin_port));
            close(new_socket);
            free(cli);
        }
    }

    return 0;
}
