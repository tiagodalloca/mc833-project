#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>

#define PORTA 5001                      // Número da porta para o servidor
#define MAX_CLIENTES 10                  // Número máximo de clientes concorrentes
#define TAMANHO_BUFFER 4096                // Tamanho do buffer de recebimento
#define PASTA_USUARIOS "../db/users/"    // Caminho pros arquivos dos usuários

volatile int rodando = 1;  // Controla o loop do servidor

int verdadeiro = 1;

void erro(const char *msg) {
    perror(msg);
    exit(1);
}

// Manipulador de sinal para SIGINT utilizado na main()
void manipulador_sigint(int signum) {
    printf("Recebido SIGINT, encerrando o servidor...\n");
    rodando = 0;
}

const char *listar_todos_usuarios(const char *caminho) {
    DIR *dir;
    struct dirent *ent;
    json_object *users_array = json_object_new_array(); // Cria lista de objetos JSON

    if ((dir = opendir(caminho)) == NULL) {
        perror("Não foi possível abrir o diretório");
        return NULL;
    }

    while ((ent = readdir(dir)) != NULL) {
        // Ignora . e ..
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

        // Lê as informações do usuário do arquivo e cria um objeto JSON.
        json_object *user_obj = json_object_new_object();
        json_object_object_add(user_obj, "email", json_object_new_string(ent->d_name));

        char buffer[10000];
        fgets(buffer, sizeof(buffer), fp);
        json_object *json_obj = json_tokener_parse(buffer);
        json_object_array_add(users_array, json_obj); // Adiciona o objeto do usuário na lista de JSON 
        fclose(fp);
    }

    closedir(dir);

    // Converte lista JSON em string e retorna
    const char *json_str = json_object_to_json_string(users_array);

    size_t len = strlen(json_str);
    char *resp = (char *) malloc(len + 1);
    strncpy(resp, json_str, len);
    resp[len] = '\0';

    json_object_put(users_array); // Libera a lista da memória

    return resp;
}

// Deleta usuário com base no email fornecido (deleta o arquivo do usuário)
void deletar_usuario(const char *email, const char *caminho) {
    char caminho_completo[100];
    sprintf(caminho_completo, "%s/%s.json", caminho, email);

    if (remove(caminho_completo) != 0) {
        printf("Não foi possível deletar o arquivo %s\n", caminho_completo);
    } else {
        printf("Arquivo %s deletado com sucesso.\n", caminho_completo);
    }
}

// Função auxiliar utilizada na busca por parâmetro
int busca_substring(const char *texto, const char *subtexto) {
    unsigned int tamanho_texto = strlen(texto);
    unsigned int tamanho_subtexto = strlen(subtexto);
    if (tamanho_texto < tamanho_subtexto) {
        return 0; // Texto é menor que o subtexto
    }
    for (size_t i = 0; i <= tamanho_texto - tamanho_subtexto; i++) {
        if (strncasecmp(texto + i, subtexto, tamanho_subtexto) == 0) {
            return 1; // Substring encontrada
        }
    }
    return 0; // Substring não encontrada
}

// Busca por parâmetro
const char *query_usuarios(const json_object *payload, const char *caminho) {
    json_object *user_array = json_object_new_array(); // Cria a lista de JSON

    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(caminho)) == NULL) {
        perror("Não foi possível abrir o diretório");
        return NULL;
    }

    while ((ent = readdir(dir)) != NULL) {
        // Ignora . e ..
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
            json_object *user = json_tokener_parse(buffer); // Parse JSON
            if (user != NULL) {
                int match = 1; // Flag que indica se usuario da match nos parâmetros do payload
                json_object_object_foreach(payload, key, val) {
                    json_object *user_value = json_object_object_get(user, key);
                    if (user_value != NULL) {
                        const char *user_value_str = json_object_get_string(user_value);
                        const char *payload_value_str = json_object_get_string(val);
                        if (strcasecmp(key, "habilidades") == 0) {
                            if (busca_substring(user_value_str, payload_value_str) == NULL) {
                                match = 0; // Para o campo habilidades se o parametro passado no payload não for SUBSTRING dele, flag = False e Break
                                break;
                            }
                        } else if (strcmp(user_value_str, payload_value_str) != 0) {
                            match = 0; // Para demais campos, se o do payload não for igual ao do usuário, flag = False e Break
                            break;
                        }
                    } else {
                        match = 0; // Caso em que o campo passado como filtro não existe não existe no usuário
                        break;
                    }
                }
                if (match) {
                    // Se deu match, adiciona usuário (nome e email) na lista de resposta
                    json_object *nome = NULL; //pega o nome
                    json_object_object_get_ex(user, "nome", &nome);

                    json_object *email = NULL; //pega o email
                    json_object_object_get_ex(user, "email", &email);

                    // Extract string values from retrieved properties
                    const char *nome_str = json_object_get_string(nome);
                    const char *email_str = json_object_get_string(email);

                    // Cria novo objeto com email e nome e adicionar na lista de resposta
                    json_object *new_user = json_object_new_object();
                    json_object_object_add(new_user, "nome", nome);
                    json_object_object_add(new_user, "email", email);

                    json_object_array_add(user_array, new_user);
                } else {
                    json_object_put(user); // da Free no JSON se não for adicionado na lista
                }
            }
        }

        fclose(fp);
    }

    closedir(dir);

    const char *json_str = json_object_to_json_string(user_array); // Converte JSON array em string

    size_t len = strlen(json_str);
    char *resp = (char *) malloc(len + 1);
    strncpy(resp, json_str, len);
    resp[len] = '\0';

    json_object_put(user_array); // Libera a lista da memória

    return resp;
}

// Retorna usuário a partir do email
const char *listar_usuario(const char *email, const char *caminho) {
    char caminho_completo[100];
    // Como o nome do arquivo é criado a partir do email do usuário, não é necessário procurar pelo usuário certo, só recriar o nome do arquivo através do email passado e abri-lo
    sprintf(caminho_completo, "%s/%s.json", caminho, email);

    FILE *fp = fopen(caminho_completo, "r");
    if (fp == NULL) {
        printf("Não foi possível abrir o arquivo %s\n", caminho_completo);
        return NULL;
    }

    char buffer[4096];
    json_object *user = NULL;

    while (fgets(buffer, sizeof(buffer), fp)) {
        user = json_tokener_parse(buffer); // Parse JSON do arquivo e coloca na variavel user
        if (user != NULL) {
            break;
        }
    }

    fclose(fp);

    if (user == NULL) {
        printf("Usuário com email %s não encontrado\n", email);
        return NULL;
    }

    const char *json_str = json_object_to_json_string(user); // Converte objeto JSON em string

    size_t len = strlen(json_str);
    char *resp = (char *) malloc(len + 1);
    strncpy(resp, json_str, len);
    resp[len] = '\0';

    json_object_put(user); // Libera JSON da memória

    return resp;
}

// Novo usuário: Cria um arquivo e armazena o json de entrada nele
void armazenar_info_usuario(json_object *json_obj, const char *caminho) {
    // Pega o email do JSON
    json_object *email_obj = NULL;
    if (json_object_object_get_ex(json_obj, "email", &email_obj) == 0) {
        printf("Campo 'email' não encontrado no objeto JSON\n");
        return;
    }

    const char *email = json_object_get_string(email_obj);

    // Cria arquivo com o email como seu nome
    char nome_arquivo[100];
    sprintf(nome_arquivo, "%s/%s.json", caminho, email);

    FILE *fp = fopen(nome_arquivo, "w");
    if (fp == NULL) {
        printf("Erro ao criar arquivo %s\n", nome_arquivo);
        return;
    }

    // Escreve objeto JSON no arquivo
    const char *json_str = json_object_to_json_string(json_obj);
    fprintf(fp, "%s", json_str);

    // Fecha arquivo
    fclose(fp);
}

// Lida com as solicitações dos clientes (função que recebe os requests dos clientes)
// recebe como entrada um JSON em formato de string.
void *lidar_com_cliente(void *arg) {
    int novo_sockfd = *((int *) arg);
    char buffer[TAMANHO_BUFFER];

    while (rodando) {
        memset(buffer, 0, sizeof(buffer));

        int n = recv(novo_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n < 0) {
            erro("ERRO lendo do socket");
            continue;
        } else if (n == 0) {
            printf("Cliente desconectado\n");
            break;

        }

        json_object *json_obj = json_tokener_parse(buffer);

        if (json_obj == NULL) {
            printf("Erro ao analisar o JSON\n");
            continue;
        }

        // Pega os parâmetros "command" and "payload" do JSON de entrada
        // "command" é a operação a ser realizada e "payload" contém os atributos a serem utilizados nas operações
        json_object *command_obj, *payload_obj;
        if (json_object_object_get_ex(json_obj, "command", &command_obj) &&
            json_object_object_get_ex(json_obj, "payload", &payload_obj)) {
            const char *command = json_object_get_string(command_obj);
            json_object *payload = payload_obj;

            if (strcmp(command, "new_user") == 0) { // Cria novo usuário
                armazenar_info_usuario(payload, PASTA_USUARIOS);
                const char *response = "Novo usuário criado com sucesso!\n";
                // Manda resposta pro cliente
                send(novo_sockfd, response, strlen(response), 0);

            } else if (strcmp(command, "list_all_users") == 0) {    // Lista todos os usuários
                const char *response = listar_todos_usuarios(PASTA_USUARIOS);
                if (response == NULL) response = "Não há usuários cadastrados\n";
                // Manda resposta pro cliente
                send(novo_sockfd, response, strlen(response), 0);

            } else if (strcmp(command, "delete_user") == 0) {   // Deleta usuário
                const char *email = json_object_get_string(payload);
                deletar_usuario(email, PASTA_USUARIOS);
                const char *response = "Usuário deletado com sucesso!\n";
                // Manda resposta pro cliente
                send(novo_sockfd, response, strlen(response), 0);

            } else if (strcmp(command, "query_users") == 0) {   // Filtra usuários por um de seus atributos
                const char *response = query_usuarios(payload, PASTA_USUARIOS);
                if (response == NULL) response = "Não achamos ninguém com esse atributo\n";
                // Manda resposta pro cliente 
                send(novo_sockfd, response, strlen(response), 0);

            } else if (strcmp(command, "list_user") == 0) {     // Devolve usuário com o devido email
                const char *email = json_object_get_string(payload);
                const char *response = listar_usuario(email, PASTA_USUARIOS);
                if (response == NULL) response = "Não achamos ninguém com esse email\n";
                // Manda resposta pro cliente 
                send(novo_sockfd, response, strlen(response), 0);

            } else {    // Comando inválido
                const char *response = "Comando inválido\n";
                send(novo_sockfd, response, strlen(response), 0);
            }

        } else {    // Entrada inválida/formato do JSON de entrada errado
            const char *response = "Entrada inválida/Formato de objeto JSON inválido\n";
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

int sockfds[MAX_CLIENTES];
pthread_t threads[MAX_CLIENTES]; // Array para armazenar IDs de threads

// Verifica se é possível e cria uma nova thread para lidar com o novo cliente
void *aceitar_novos_clientes() {
    while (rodando) {
        int novo_sockfd;
        clilen = (socklen_t) sizeof(cli_addr);
        novo_sockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (novo_sockfd < 0)
            erro("ERRO ao aceitar");

        if (num_clients >= MAX_CLIENTES) {
            printf("Número máximo de clientes alcançado, fechando a conexão...\n");
            close(novo_sockfd);
        } else {
            // Cria thread para lidar com a solicitação do cliente
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

//Função teste que imprime todos os usuários cadastrados
void imprimir_arquivos_na_pasta(const char *caminho) {
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

        FILE *fp = fopen(caminho_completo, "r");
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
    signal(SIGINT, manipulador_sigint); // Registra manipulador de sinal SIGINT
    printf("Iniciando o servidor e printando DB atual...\n");

    imprimir_arquivos_na_pasta(PASTA_USUARIOS); //Chamada teste

    // Cria um socket TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        erro("ERRO abrindo o socket");

    // Configura a estrutura do endereço do servidor
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORTA);

    // Conecta o socket ao endereço do servidor
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        erro("ERRO conectando");

    // Inicia a escuta do socket
    listen(sockfd, 5);

    printf("Servidor iniciado na porta %d\n", PORTA);

    // Criar thread para aceitar novos clientes
    pthread_t accept_thread;
    int ret = pthread_create(&accept_thread, NULL, aceitar_novos_clientes, NULL);
    if (ret != 0) {
        printf("Falha ao criar thread de aceitação de clientes, encerrando o servidor...\n");
        close(sockfd);
        exit(1);
    }

    // Aguarda a sinalização para encerrar o servidor
    while (rodando) sleep(1);

    printf("Encerrando o servidor...\n");

    // Fecha conexões com os clientes
    for (int i = 0; i < num_clients; i++) {
        pthread_cancel(threads[i]);
        close(sockfds[i]);
    }

    // Fecha o socket do servidor
    close(sockfd);

    return 0;
}
