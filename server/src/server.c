#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>

#define PORT 5000                       // Número da porta para o servidor
#define MAX_CLIENTS 10                  // Número máximo de clientes concorrentes
#define BUFFER_SIZE 4096                // Tamanho do buffer de recebimento
#define DB_USERS_PATH "../db/users/"    // Caminho pros arquivos dos usuários

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

const char *listar_todos_usuarios(const char *caminho) {
    DIR *dir;
    struct dirent *ent;
    json_object *users_array = json_object_new_array(); // Create JSON array object

    if ((dir = opendir(caminho)) == NULL) {
        perror("Não foi possível abrir o diretório");
        return NULL;
    }

    while ((ent = readdir(dir)) != NULL) {
        // Ignorar . e ..
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        char caminho_completo[100];
        sprintf(caminho_completo, "%s/%s", caminho, ent->d_name);

        FILE *fp = fopen(caminho_completo, "r");
        if (fp == NULL) {
            printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
            continue;
        }

        // Read user information from file and create JSON object
        json_object *user_obj = json_object_new_object();
        json_object_object_add(user_obj, "email", json_object_new_string(ent->d_name));

        char buffer[10000];
        fgets(buffer, sizeof(buffer), fp);
        json_object *json_obj = json_tokener_parse(buffer);
        json_object_array_add(users_array, json_obj); // Add user object to JSON array
        fclose(fp);
    }

    closedir(dir);

    // Convert JSON array to string and return
    const char *json_str = json_object_to_json_string(users_array);

    size_t len = strlen(json_str);
    char *resp = (char *)malloc(len + 1);
    strncpy(resp, json_str, len);
    resp[len] = '\0';

    json_object_put(users_array); // Release JSON array memory

    return resp;
}

// Function to delete user file based on email
void deletar_usuario(const char *email, const char *caminho) {
    char caminho_completo[100];
    sprintf(caminho_completo, "%s/%s.json", caminho, email);

    if (remove(caminho_completo) != 0) {
        printf("Não foi possível deletar o arquivo %s\n", caminho_completo);
    } else {
        printf("Arquivo %s deletado com sucesso.\n", caminho_completo);
    }
}

int busca_substring(const char *texto, const char *subtexto) {
    unsigned int tamanho_texto = strlen(texto);
    unsigned int tamanho_subtexto = strlen(subtexto);
    if (tamanho_texto < tamanho_subtexto) {
        return 0; // Retorna falso se o texto é menor que o subtexto
    }
    for (size_t i = 0; i <= tamanho_texto - tamanho_subtexto; i++) {
        if (strncasecmp(texto + i, subtexto, tamanho_subtexto) == 0) {
            return 1; // Retorna verdadeiro se encontrar uma correspondência de substring
        }
    }
    return 0; // Retorna falso se nenhuma correspondência de substring for encontrada
}

const char *query_usuarios(const json_object *payload, const char *caminho) {
    json_object *user_array = json_object_new_array(); // Create a new JSON array

    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(caminho)) == NULL) {
        perror("Não foi possível abrir o diretório");
        return NULL;
    }

    while ((ent = readdir(dir)) != NULL) {
        // Ignorar . e ..
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        char caminho_completo[100];
        sprintf(caminho_completo, "%s/%s", caminho, ent->d_name);

        FILE *fp = fopen(caminho_completo, "r");
        if (fp == NULL) {
            printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
            continue;
        }

        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), fp)) {
            json_object *user = json_tokener_parse(buffer); // Parse JSON from file
            if (user != NULL) {
                int match = 1; // Flag to indicate if user matches fields in payload
                json_object_object_foreach(payload, key, val) {
                    json_object *user_value = json_object_object_get(user, key);
                    if (user_value != NULL) {
                        const char *user_value_str = json_object_get_string(user_value);
                        const char *payload_value_str = json_object_get_string(val);
                        if (strcasecmp(key, "habilidades") == 0) {
                            if (busca_substring(user_value_str, payload_value_str) == NULL) {
                                match = 0; // Set match flag to false if habilidades do not partially match
                                break; // Break loop if a mismatch is found
                            }
                        }
                        else if (strcmp(user_value_str, payload_value_str) != 0) {
                            match = 0; // Set match flag to false if values are not equal
                            break; // Break loop if a mismatch is found
                        }
                    } else {
                        match = 0; // Set match flag to false if key not found in user
                        break; // Break loop if key not found in user
                    }
                }
                if (match) {
                    // Add user to the JSON array
                    json_object_array_add(user_array, user);
                } else {
                    json_object_put(user); // Free JSON object if not added to array
                }
            }
        }

        fclose(fp);
    }

    closedir(dir);

    const char *json_str = json_object_to_json_string(user_array); // Convert JSON array to string

    size_t len = strlen(json_str);
    char *resp = (char *)malloc(len + 1);
    strncpy(resp, json_str, len);
    resp[len] = '\0';

    json_object_put(user_array); // Release JSON array memory

    return resp;
}

const char *listar_usuario(const char *email, const char *caminho) {
    char caminho_completo[100];
    sprintf(caminho_completo, "%s/%s.json", caminho, email); // Create complete file path for the given email

    FILE *fp = fopen(caminho_completo, "r");
    if (fp == NULL) {
        printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
        return NULL;
    }

    char buffer[4096];
    json_object *user = NULL;

    while (fgets(buffer, sizeof(buffer), fp)) {
        user = json_tokener_parse(buffer); // Parse JSON from file
        if (user != NULL) {
            break; // Break loop if user is found
        }
    }

    fclose(fp);

    if (user == NULL) {
        printf("Usuário com email %s não encontrado\n", email);
        return NULL;
    }

    const char *json_str = json_object_to_json_string(user); // Convert JSON object to string

    size_t len = strlen(json_str);
    char *resp = (char *)malloc(len + 1);
    strncpy(resp, json_str, len);
    resp[len] = '\0';

    json_object_put(user); // Release JSON object memory

    return resp;
}

// Cria um arquivo e armazena o json de entrada nele
void armazenar_info_usuario(json_object *json_obj, const char *caminho) {
    // Get email from json object
    json_object *email_obj = NULL;
    if (json_object_object_get_ex(json_obj, "email", &email_obj) == 0) {
        printf("Campo 'email' não encontrado no objeto JSON\n");
        return;
    }

    const char *email = json_object_get_string(email_obj);

    // Criar arquivo com o email como nome do arquivo
    char nome_arquivo[100];
    sprintf(nome_arquivo, "%s/%s.json", caminho, email);

    FILE *fp = fopen(nome_arquivo, "w");
    if (fp == NULL) {
        printf("Erro ao criar arquivo %s\n", nome_arquivo);
        return;
    }

    // Escrever objeto JSON no arquivo
    const char *json_str = json_object_to_json_string(json_obj);
    fprintf(fp, "%s", json_str);

    // Fechar arquivo
    fclose(fp);
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

        // Parse JSON from received buffer
        json_object *json_obj = json_tokener_parse(buffer);

        if (json_obj == NULL) {
            printf("Erro ao analisar o JSON\n");
            continue;
        }

        // Extract "command" and "payload" from JSON
        json_object *command_obj, *payload_obj;
        if (json_object_object_get_ex(json_obj, "command", &command_obj) &&
            json_object_object_get_ex(json_obj, "payload", &payload_obj)) {
            const char *command = json_object_get_string(command_obj);
            json_object *payload = payload_obj;

            if (strcmp(command, "new_user") == 0) {
                // Create new user
                armazenar_info_usuario(payload, DB_USERS_PATH);
                // Send response to client
                const char *response = "Novo usuário criado com sucesso!\n";
                send(novo_sockfd, response, strlen(response), 0);
            } else if (strcmp(command, "list_all_users") == 0) {
                // List all users
                const char *response = listar_todos_usuarios(DB_USERS_PATH);
                // Send response to client
                send(novo_sockfd, response, strlen(response), 0);
            } else if (strcmp(command, "delete_user") == 0) {
                // Delete user
                const char *email = json_object_get_string(payload);
                deletar_usuario(email, DB_USERS_PATH);
                // Send response to client
                const char *response = "Usuário deletado com sucesso!\n";
                send(novo_sockfd, response, strlen(response), 0);
            } else if (strcmp(command, "query_users") == 0) {
                // Query users
                const char *response = query_usuarios(payload, DB_USERS_PATH);
                send(novo_sockfd, response, strlen(response), 0);
            } else if (strcmp(command, "list_user") == 0) {
                // List user
                const char *email = json_object_get_string(payload);
                const char *response = listar_usuario(email, DB_USERS_PATH);
                if (response == NULL) response = "Não achamos ninguém com esse email\n";
                send(novo_sockfd, response, strlen(response), 0);
            } else {
                // Invalid command
                const char *response = "Comando inválido\n";
                send(novo_sockfd, response, strlen(response), 0);
            }

        } else {
            // Invalid JSON format
            const char *response = "Formato de objeto JSON inválido";
            send(novo_sockfd, response, strlen(response), 0);
        }

        json_object_put(json_obj);
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

void imprimir_arquivos_na_pasta(const char* caminho) {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(caminho)) == NULL) {
        perror("Não foi possível abrir o diretório");
        return;
    }

    while ((ent = readdir(dir)) != NULL) {
        // Ignorar . e ..
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        char caminho_completo[100];
        sprintf(caminho_completo, "%s/%s", caminho, ent->d_name);

        FILE* fp = fopen(caminho_completo, "r");
        if (fp == NULL) {
            printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
            continue;
        }

        printf("Arquivo: %s\n", caminho_completo);

        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), fp)) {
            printf("%s", buffer);
        }

        printf("\n");
        fclose(fp);
    }

    closedir(dir);
}

int main() {
    signal(SIGINT, manipulador_sigint); // Registrar manipulador de sinal SIGINT
    printf("Iniciando o servidor e printando DB atual...\n");

    imprimir_arquivos_na_pasta(DB_USERS_PATH); //colocar aqui o path pra pasta de users(DB_USERS_PATH); //colocar aqui o path pra pasta de users

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
