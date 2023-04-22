#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>

#define PORT 5001          // Número da porta para o servidor
#define MAX_CLIENTS 10     // Número máximo de clientes concorrentes
#define BUFFER_SIZE 4096   // Tamanho do buffer de recebimento

volatile int running = 1;  // Variável para controlar o loop do servidor

int verdadeiro = 1;

void erro(const char *msg) {
    perror(msg);
    exit(1);
}

// Manipulador de sinal para SIGINT
void manipulador_sigint(int signum) {
    printf("Recebido SIGINT, encerrando o servidor...\n");
    running = 0;
}

// Função para lidar com as solicitações dos clientes
void *lidar_com_cliente(void *arg) {
    int novo_sockfd = *((int *) arg);
    char buffer[BUFFER_SIZE];

    while (running) {
        memset(buffer, 0, sizeof(buffer));

        int n = recv(novo_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n < 0) {
            erro("ERRO lendo do socket");
            continue;
        } else if (n == 0) {
            printf("Cliente desconectado\n");
            continue;

        }

        // Analisar objeto JSON
        json_object *json_obj = json_tokener_parse(buffer);
        if (json_obj == NULL) {
            printf("Objeto JSON inválido.\n");
            continue;
        }

        // Extrair campos de nome e e-mail
        json_object *nome_obj, *email_obj;
        if (json_object_object_get_ex(json_obj, "nome", &nome_obj) &&
            json_object_object_get_ex(json_obj, "email", &email_obj)) {
            const char *nome = json_object_get_string(nome_obj);
            const char *email = json_object_get_string(email_obj);

            printf("Objeto JSON recebido:\n");
            printf("Nome: %s\n", nome);
            printf("Email: %s\n", email);
        } else {
            printf("Formato de objeto JSON inválido\n");
        }

        json_object_put(json_obj); // Liberar objeto JSON

        if (n == 0)
            break;
    }

    close(novo_sockfd);

    return NULL;
}

unsigned int clilen;
struct sockaddr_in serv_addr, cli_addr;
int sockfd, num_clients;

int sockfds[MAX_CLIENTS];
pthread_t threads[MAX_CLIENTS]; // Array para armazenar IDs de threads

void *aceitar_novos_clientes() {
    while (running) {
        int novo_sockfd;
        clilen = (socklen_t) sizeof(cli_addr);
        novo_sockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (novo_sockfd < 0)
            erro("ERRO ao aceitar");

        if (num_clients >= MAX_CLIENTS) {
            printf("Número máximo de clientes alcançado, fechando a conexão...\n");
            close(novo_sockfd);
        } else {
            // Criar thread para lidar com a solicitação do cliente
            int ret = pthread_create(&threads[num_clients], NULL, lidar_com_cliente, &novo_sockfd);
            if (ret != 0) {
                printf("Falha ao criar thread para o cliente, fechando a conexão...\n");
                close(novo_sockfd);
            } else {
                printf("Cliente conectado, criada nova thread para lidar com a solicitação\n");
                sockfds[num_clients] = novo_sockfd;
                num_clients++;
            }
        }
    }
    return NULL;
}

int main() {
    signal(SIGINT, manipulador_sigint); // Registrar manipulador de sinal SIGINT
    printf("Iniciando o servidor...\n");

    // Criar um socket TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        erro("ERRO abrindo o socket");

    // Configurar a estrutura do endereço do servidor
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Conectar o socket ao endereço do servidor
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        erro("ERRO conectando");

    // Iniciar a escuta do socket
    listen(sockfd, 5);

    printf("Servidor iniciado na porta %d\n", PORT);

    // Criar thread para aceitar novos clientes
    pthread_t accept_thread;
    int ret = pthread_create(&accept_thread, NULL, aceitar_novos_clientes, NULL);
    if (ret != 0) {
        printf("Falha ao criar thread de aceitação de clientes, encerrando o servidor...\n");
        close(sockfd);
        exit(1);
    }

    // Aguardar a sinalização para encerrar o servidor
    while (running) sleep(1);

    printf("Encerrando o servidor...\n");

    // Fechar conexões com os clientes
    for (int i = 0; i < num_clients; i++) {
        pthread_cancel(threads[i]);
        close(sockfds[i]);
    }

    // Fechar o socket do servidor
    close(sockfd);

    return 0;
}
