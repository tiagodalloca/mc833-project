#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 5000 // Port number of the server
#define MAX_BUFFER_SIZE 1024 // Maximum buffer size for messages
#define SERVER_ADDRESS "127.0.0.1"

int main(int argc, char *argv[]) {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[MAX_BUFFER_SIZE] = {0};

    // Creating socket file descriptor
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Set server address parameters
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_ADDRESS, &server_address.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    ////////////////

    char action[20];
    char payload[500] = "{}";
    char email[50];
    char nome[50];
    char sobrenome[50];
    char cidade[50];
    char formacao[50];
    char ano[10];
    char habilidades[500];
    int bytes_received;

    printf("Bem vindo ao cadastro de Usuários mc833, selecione uma das operações abaixo para continuar:\n");
    printf("1. Cadastrar novo usuário\n");
    printf("2. Listar todos os usuários\n");
    printf("3. Buscar por um usuário\n");
    printf("4. Listar todas as pessoas formadas em certo curso\n");
    printf("5. Listar todas as pessoas formadas em certo ano\n");
    printf("6. Listar todas as pessoas que possuem certa habilidade\n");
    printf("7. Remover um usuário\n");
    printf("8. Sair\n");

    char input[10];

    while (1)
    {
        scanf("%s", input);

        if (strcmp(input, "1") == 0) {
            strcpy(action, "new_user");
            printf("email: ");
            scanf("%s", email);
            printf("nome: ");
            scanf("%s", nome);
            printf("sobrenome: ");
            scanf("%s", sobrenome);
            printf("cidade de residência: ");
            scanf("%s", cidade);
            printf("formação acadêmica: ");
            scanf("%s", formacao);
            printf("ano de formatura: ");
            scanf("%s", ano);
            printf("habilidades: ");
            scanf("%s", habilidades);

            sprintf(payload, "{ \"nome\": \"%s %s\", \"sobrenome\": \"%s\", \"cidade\": \"%s\", \"formacao\": \"%s\", \"ano\": \"%s\", \"email\": \"%s\", \"habilidades\": \"%s\" }", nome, sobrenome, sobrenome, cidade, formacao, ano, email, habilidades);
            

        } else if (strcmp(input, "2") == 0) {
            strcpy(action, "list_all_users");
            strcpy(payload, "{}");

        } else if (strcmp(input, "3") == 0) {
            strcpy(action, "list_user");
            printf("Email do usuário que procura: ");
            scanf("%s", email);
            sprintf(payload, "{ \"%s\" }", email);

        } else if (strcmp(input, "4") == 0) {
            strcpy(action, "query_users");
            printf("Nome do curso: ");
            scanf("%s", formacao);
            sprintf(payload, "{ \"formacao\": \"%s\" }", formacao);

        } else if (strcmp(input, "5") == 0) {
            strcpy(action, "query_users");
            printf("Ano de formação: ");
            scanf("%s", ano);
            sprintf(payload, "{ \"ano\": \"%s\" }", ano);

        } else if (strcmp(input, "6") == 0) {
            strcpy(action, "query_users");
            printf("Nome da habilidade: ");
            scanf("%s", habilidades);
            sprintf(payload, "{ \"habilidades\": \"%s\" }", habilidades);

        } else if (strcmp(input, "7") == 0) {
            strcpy(action, "delete_user");
            printf("Email do usuário que deseja deletar: ");
            scanf("%s", email);
            sprintf(payload, "{ \"%s\" }", email);

        } else if (strcmp(input, "8") == 0) {   //comando de saída para quebrar o loop
            break

        } else {
            printf("Opção inválida");
        }
        if (strcmp(input, "1") == 0 || strcmp(input, "2") == 0 || strcmp(input, "3") == 0 || strcmp(input, "4") == 0 || strcmp(input, "5") == 0 || strcmp(input, "6") == 0 || strcmp(input, "7") == 0) {
            send(client_socket, payload, strlen(payload), 0);
            bytes_received = read(client_socket, buffer, MAX_BUFFER_SIZE);
            printf(" %s\n", buffer);
        }
    }
    
    
    /////////////////

    // Send message to server
    //char *message = "Hello from client!";
    //send(client_socket, message, strlen(message), 0);
    //printf("Message sent to server: %s\n", message);

    // Receive message from server
    //int bytes_received = read(client_socket, buffer, MAX_BUFFER_SIZE);
    //printf("Message received from server: %s\n", buffer);

    close(client_socket);   // Termina a conexão

    return 0;
}