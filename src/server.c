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

// for phase 3
#include <pthread.h>

#define PORT 8081
#define BUFFER_SIZE 2048

//since we will keep track of the thread numbers assigned to each client
int client_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;


void* handle_client(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);  // clean up dynamic memory

    // get the ip and port of the client to log these
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    int client_number;

    // assign a thread number to the client connecting
    pthread_mutex_lock(&counter_mutex);
    client_counter++;
    client_number = client_counter;
    pthread_mutex_unlock(&counter_mutex);

    printf("[INFO] Client #%d connected from %s:%d. Assigned to Thread-%d.\n",
           client_number, client_ip, client_port, client_number);

    char clientCommand[BUFFER_SIZE];
    char *parsedCommand[50];
    int argCount;

    while (1) {
        memset(clientCommand, 0, sizeof(clientCommand));
        int bytesReceived = recv(client_socket, clientCommand, sizeof(clientCommand), 0);

        if (bytesReceived <= 0) {
            printf("[INFO] Client #%d - %s:%d disconnected.\n", client_number, client_ip, client_port);
            break;
        }

        printf("[RECEIVED] [Client #%d - %s:%d] Received command: \"%s\"\n",
               client_number, client_ip, client_port, clientCommand);

        if (strcmp(clientCommand, "exit") == 0) {
            printf("[INFO] [Client #%d - %s:%d] Client requested disconnect. Closing connection.\n",
                   client_number, client_ip, client_port);
            break;
        }

        parseInput(clientCommand, parsedCommand, &argCount);
        if (parsedCommand[0] == NULL) continue;

        int pipeFound = 0, redirectFound = 0;
        int inputRedirectFound = 0, outputRedirectFound = 0, errorRedirectFound = 0;

        for (int j = 0; j < argCount; j++) {
            if (strcmp(parsedCommand[j], "|") == 0) pipeFound = 1;
            if (strcmp(parsedCommand[j], "<") == 0) { redirectFound = 1; inputRedirectFound = 1; }
            if (strcmp(parsedCommand[j], ">") == 0) { redirectFound = 1; outputRedirectFound = 1; }
            if (strcmp(parsedCommand[j], "2>") == 0 || strcmp(parsedCommand[j], "2>&1") == 0) {
                redirectFound = 1; errorRedirectFound = 1;
            }
        }

        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("[ERROR] Pipe creation failed");
            continue;
        }

        printf("[EXECUTING] [Client #%d - %s:%d] Executing command: \"%s\"\n",
               client_number, client_ip, client_port, clientCommand);

        //for all the commands
        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            if (pipeFound && redirectFound) {
                handlePipeRedirect(parsedCommand);
            } else if (pipeFound) {
                int pipeCount = 0;
                for (int j = 0; j < argCount; j++) {
                    if (strcmp(parsedCommand[j], "|") == 0) pipeCount++;
                }
                if (pipeCount > 1) {
                    handleMultiplePipes(parsedCommand);
                } else {
                    handlePipes(parsedCommand);
                }
            } else if (redirectFound) {
                if ((inputRedirectFound && outputRedirectFound) || 
                    (inputRedirectFound && errorRedirectFound) || 
                    (outputRedirectFound && errorRedirectFound)) {
                    handleCombinedRedirect(parsedCommand);
                } else if (inputRedirectFound) {
                    inputRedirect(parsedCommand);
                } else {
                    handleRedirect(parsedCommand);
                }
            } else {
                noArgCommand(parsedCommand);
            }

            exit(0);
        } else {
            close(pipefd[1]);

            char response[BUFFER_SIZE];
            memset(response, 0, BUFFER_SIZE);
            read(pipefd[0], response, BUFFER_SIZE - 1);
            close(pipefd[0]);

            int status;
            waitpid(pid, &status, 0);

            if (strlen(response) == 0) {
                strcpy(response, "\n");
            } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                printf("[ERROR] [Client #%d - %s:%d] Command not found: \"%s\"\n",
                       client_number, client_ip, client_port, clientCommand);
                strcpy(response, "Command not found.\n");
            }

            printf("[OUTPUT] [Client #%d - %s:%d] Sending output to client:\n%s",
                   client_number, client_ip, client_port, response);

            send(client_socket, response, strlen(response), 0);
        }
    }

    close(client_socket);
    pthread_exit(NULL);
}




int main() {
    int server_socket;
    struct sockaddr_in server_address, client_address;
    int opt = 1;

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("[ERROR] Socket creation failed");
        exit(1);
    }

    // allow the socket to be reused
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[ERROR] setsockopt failed");
        exit(1);
    }

    //set the server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    //bind it to the address
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("[ERROR] Socket bind failed");
        exit(EXIT_FAILURE);
    }

    // now listen for incoming connections
    if (listen(server_socket, 5) < 0) {
        perror("[ERROR] Socket listen failed");
        exit(EXIT_FAILURE);
    }

    printf("[INFO] Server started, waiting for client connections...\n");

    while (1) {
        socklen_t addrlen = sizeof(client_address);
        int* client_socket = malloc(sizeof(int)); // dynamically allocated for each thread
        *client_socket = accept(server_socket, (struct sockaddr*)&client_address, &addrlen);

        if (*client_socket < 0) {
            perror("[ERROR] Socket accept failed");
            free(client_socket);
            continue;
        }

        printf("[INFO] New client connected.\n");

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_socket) != 0) {
            perror("[ERROR] pthread_create failed");
            free(client_socket);
        }

        pthread_detach(tid); // to ensure resources are released when thread terminates
    }

    close(server_socket);
    return 0;
}

