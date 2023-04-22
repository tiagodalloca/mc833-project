#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>

#define PORT 5001          // Port number for the server
#define MAX_CLIENTS 10     // Maximum number of concurrent clients
#define BUFFER_SIZE 4096   // Size of receive buffer

volatile int running = 1;  // Variable to control server loop

int true = 1;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Signal handler for SIGINT
void sigint_handler(int signum) {
    printf("Received SIGINT, shutting down server...\n");
    running = 0;
}

// Function to handle client requests
void *handle_client(void *arg) {
    int newsockfd = *((int *) arg);
    char buffer[BUFFER_SIZE];

    while (running) {
        memset(buffer, 0, sizeof(buffer));

        int n = recv(newsockfd, buffer, sizeof(buffer) - 1, 0);
        if (n < 0) {
            error("ERROR reading from socket");
            continue;
        } else if (n == 0) {
            printf("Client disconnected\n");
            continue;

        }

        // Parse JSON object
        json_object *json_obj = json_tokener_parse(buffer);
        if (json_obj == NULL) {
            printf("Malformed JSON object.\n");
            continue;
        }

        // Extract name and email fields
        json_object *name_obj, *email_obj;
        if (json_object_object_get_ex(json_obj, "name", &name_obj) &&
            json_object_object_get_ex(json_obj, "email", &email_obj)) {
            const char *name = json_object_get_string(name_obj);
            const char *email = json_object_get_string(email_obj);

            printf("Received JSON object:\n");
            printf("Name: %s\n", name);
            printf("Email: %s\n", email);
        } else {
            printf("Invalid JSON object format\n");
        }

        json_object_put(json_obj); // Release JSON object

        if (n == 0)
            break;
    }

    close(newsockfd);

    return NULL;
}

unsigned int clilen;
struct sockaddr_in serv_addr, cli_addr;
int sockfd, num_clients;

int sockfds[MAX_CLIENTS];
pthread_t threads[MAX_CLIENTS]; // Array to store thread IDs

void *accept_new_clients() {
    while (running) {
        int newsockfd;
        clilen = (socklen_t) sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");

        if (num_clients >= MAX_CLIENTS) {
            printf("Maximum number of clients reached, closing connection...\n");
            close(newsockfd);
        } else {
            // Create a new thread to handle the client request
            int ret = pthread_create(&threads[num_clients], NULL, handle_client, &newsockfd);
            if (ret != 0) {
                printf("Failed to create thread for client, closing connection...\n");
                close(newsockfd);
            } else {
                printf("Client connected, thread created successfully!\n");
                sockfds[num_clients] = newsockfd;
                num_clients++;
            }
        }
    }
}

int main() {
    // Number of active threads
    num_clients = 0;

    // Register SIGINT signal handler
    signal(SIGINT, sigint_handler);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 5);

    printf("Server listening on port %d...\n", PORT);

    pthread_t accept_new_clients_thread;
    pthread_create(&accept_new_clients_thread, NULL, accept_new_clients, NULL);

    while(running) {
        // do nothing, just wait
    }

    pthread_cancel(accept_new_clients_thread);

    // Closes clients' connections
    for (int i = 0; i < num_clients; i++) {
        close(sockfds[i]);
    }

    // Closes threads
    for (int i = 0; i < num_clients; i++) {
        pthread_cancel(threads[i]);
    }

    // Signal to kernel it can reuse the address
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int));
    close(sockfd);

    printf("Server shut down successfully.\n");

    return 0;
}
