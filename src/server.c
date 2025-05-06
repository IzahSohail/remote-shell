#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "executor.h"
#include "parser.h"
#include "scheduler.h"

// for phase 3
#include <pthread.h>

#define PORT 8081
#define BUFFER_SIZE 32767

// since we will keep track of the thread numbers assigned to each client
int client_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    int client_number;
    pthread_mutex_lock(&counter_mutex);
    client_counter++;
    client_number = client_counter;
    pthread_mutex_unlock(&counter_mutex);

    printf("[INFO] Client #%d connected from %s:%d. Assigned to Thread-%d.\n",
           client_number, client_ip, client_port, client_number);

    char clientCommand[BUFFER_SIZE];
    char commandCopy[BUFFER_SIZE];  // Add a copy for parsing
    char *parsedCommand[50];
    int argCount;

    while (1) {
        memset(clientCommand, 0, sizeof(clientCommand));
        memset(commandCopy, 0, sizeof(commandCopy));
        int bytesReceived = recv(client_socket, clientCommand, sizeof(clientCommand), 0);

        if (bytesReceived <= 0) {
            printf("[INFO] Client #%d - %s:%d disconnected.\n", client_number, client_ip, client_port);
            remove_tasks_by_client(client_number);
            break;
        }

        printf("[RECEIVED] [Client #%d - %s:%d] Received command: \"%s\"\n",
               client_number, client_ip, client_port, clientCommand);

        if (strcmp(clientCommand, "exit") == 0) {
            printf("[INFO] [Client #%d - %s:%d] Client requested disconnect.\n",
                   client_number, client_ip, client_port);
            remove_tasks_by_client(client_number);
            break;
        }

        // Make a copy before parsing since parseInput modifies the string
        strncpy(commandCopy, clientCommand, sizeof(commandCopy) - 1);
        parseInput(commandCopy, parsedCommand, &argCount);
        
        if (parsedCommand[0] == NULL) continue;

        // Check if it's a demo task like "./demo 5"
        if ((strcmp(parsedCommand[0], "./demo") == 0 || strcmp(parsedCommand[0], "demo") == 0) && argCount == 2) {
            int burst_time = atoi(parsedCommand[1]);
            if (burst_time > 0) {
                add_task_with_socket(clientCommand, client_number, burst_time, 0, client_socket);  // 0 = non-shell
                continue;
            } else {
                char *err = "Usage: ./demo <burst_time>\n";
                send(client_socket, err, strlen(err), 0);
                continue;
            }
        }

        // Otherwise it's a shell command - use the original command string
        add_task_with_socket(clientCommand, client_number, -1, 1, client_socket);  // 1 = shell command

        printf("[EXECUTING] [Client #%d - %s:%d] Scheduled command: \"%s\"\n",
               client_number, client_ip, client_port, clientCommand);
    }

    close(client_socket);
    pthread_exit(NULL);
}


int main()
{
    int server_socket;
    struct sockaddr_in server_address, client_address;
    int opt = 1;

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("[ERROR] Socket creation failed");
        exit(1);
    }

    // allow the socket to be reused
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("[ERROR] setsockopt failed");
        exit(1);
    }

    // set the server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // bind it to the address
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("[ERROR] Socket bind failed");
        exit(EXIT_FAILURE);
    }

    // now listen for incoming connections
    if (listen(server_socket, 5) < 0)
    {
        perror("[ERROR] Socket listen failed");
        exit(EXIT_FAILURE);
    }

    printf("[INFO] Server started, waiting for client connections...\n");

    init_scheduler();

    while (1)
    {
        socklen_t addrlen = sizeof(client_address);
        int *client_socket = malloc(sizeof(int)); // dynamically allocated for each thread
        *client_socket = accept(server_socket, (struct sockaddr *)&client_address, &addrlen);

        if (*client_socket < 0)
        {
            perror("[ERROR] Socket accept failed");
            free(client_socket);
            continue;
        }

        printf("[INFO] New client connected.\n");

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_socket) != 0)
        {
            perror("[ERROR] pthread_create failed");
            free(client_socket);
        }

        pthread_detach(tid); // to ensure resources are released when thread terminates
    }

    close(server_socket);
    return 0;
}