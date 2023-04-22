#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 5001          // Port number for the server
#define MAX_CLIENTS 10     // Maximum number of concurrent clients
#define BUFFER_SIZE 4096   // Size of receive buffer

void error(const char *msg)
{
  perror(msg);
  exit(1);
}

// Function to handle client requests
void *handle_client(void *arg)
{
  int newsockfd = *((int *)arg);
  char buffer[BUFFER_SIZE];

  while (1)
    {
      memset(buffer, 0, sizeof(buffer));

      int n = recv(newsockfd, buffer, sizeof(buffer) - 1, 0);
      if (n < 0)
        {
          error("ERROR reading from socket");
          break;
        }
      else if (n == 0)
        {
          printf("Client disconnected\n");
          break;
        }

      // Parse JSON object
      json_object *json_obj = json_tokener_parse(buffer);
      if (json_obj == NULL)
        {
          printf("Malformed JSON object, closing connection...\n");
          break;
        }

      // Extract name and email fields
      json_object *name_obj, *email_obj;
      if (json_object_object_get_ex(json_obj, "name", &name_obj) &&
          json_object_object_get_ex(json_obj, "email", &email_obj))
        {
          const char *name = json_object_get_string(name_obj);
          const char *email = json_object_get_string(email_obj);

          printf("Received JSON object:\n");
          printf("Name: %s\n", name);
          printf("Email: %s\n", email);
        }
      else
        {
          printf("Invalid JSON object format\n");
        }

      json_object_put(json_obj); // Release JSON object
    }

  close(newsockfd);

  return NULL;
}

int main()
{
  int sockfd, newsockfd, clilen;
  struct sockaddr_in serv_addr, cli_addr;
  pthread_t threads[MAX_CLIENTS]; // Array to store thread IDs
  int num_threads = 0;            // Number of active threads

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

  memset((char *)&serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(PORT);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");

  listen(sockfd, 5);

  printf("Server listening on port %d...\n", PORT);

  while (1)
    {
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
      if (newsockfd < 0)
        error("ERROR on accept");

      if (num_threads >= MAX_CLIENTS)
        {
          printf("Maximum number of clients reached, closing connection...\n");
          close(newsockfd);
        }
      else
        {
          // Create a new thread to handle the client request
          if (pthread_create(&threads[num_threads], NULL, handle_client, &newsockfd) != 0)
            error("ERROR creating thread");

          printf("Client connected, spawning thread %ld\n", threads[num_threads]);

          // Detach the thread so it doesn't need to be explicitly joined
          pthread_detach(threads[num_threads]);
          num_threads++;
        }
    }
  
  close(sockfd);

  return 0;
}

