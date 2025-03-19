#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "executor.h"  
#include "parser.h"    

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address;
    int opt = 1;
    char clientCommand[BUFFER_SIZE];
    char *parsedCommand[50]; 
    int argCount;

    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    //check for fail error
    if (server_socket == -1) {
        perror("[ERROR] Socket creation failed");
        exit(1);
    }

    // enable reuse of address and port
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[ERROR] setsockopt failed");
        exit(1);
    }

    // Define server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("[ERROR] Socket bind failed");
        exit(EXIT_FAILURE);
    }

    //Listen for client connections
    if (listen(server_socket, 5) < 0) {
        perror("[ERROR] Socket listen failed");
        exit(EXIT_FAILURE);
    }
    printf("[INFO] Server started, waiting for client connections...\n");

    //Accept client connection
    int addrlen = sizeof(server_address);
    client_socket = accept(server_socket, (struct sockaddr*)&server_address, (socklen_t*)&addrlen);
    if (client_socket < 0) {
        perror("[ERROR] Socket accept failed");
        exit(EXIT_FAILURE);
    }
    printf("[INFO] Client connected.\n");

    // Read commands from client
    while (1) {
        memset(clientCommand, 0, sizeof(clientCommand));
        int bytesReceived = recv(client_socket, clientCommand, sizeof(clientCommand), 0);

        if (bytesReceived <= 0) {
            printf("[INFO] Client disconnected.\n");
            break;
        }

        // Print received command
        printf("[RECEIVED] Received command: \"%s\" from client.\n", clientCommand);

        // Exit server when client sends "exit"
        if (strcmp(clientCommand, "exit") == 0) {
            printf("[INFO] Closing connection.\n");
            break;
        }

        // Parse the command
        parseInput(clientCommand, parsedCommand, &argCount);

        // Prevent segmentation fault if user just presses enter!
        if (parsedCommand[0] == NULL) {
            continue;
        }

        // Execute command & capture output
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("[ERROR] Pipe failed");
            continue;
        }

        printf("[EXECUTING] Executing command: \"%s\"\n", clientCommand);

        pid_t pid = fork();
        if (pid == 0) {  // Child process
            close(pipefd[0]);  // Close read end
            dup2(pipefd[1], STDOUT_FILENO);  
            dup2(pipefd[1], STDERR_FILENO);  
            close(pipefd[1]);

            // Execute command using the executor functions
            if (argCount > 1 && strcmp(parsedCommand[1], ">") == 0) {
                handleRedirect(parsedCommand);
            } else if (argCount > 1 && strcmp(parsedCommand[1], "<") == 0) {
                inputRedirect(parsedCommand);
            } else if (argCount > 1 && strcmp(parsedCommand[1], "|") == 0) {
                handlePipes(parsedCommand);
            } else {
                noArgCommand(parsedCommand);
            }

            exit(0);
        } else {  // parent process
            close(pipefd[1]);  //close write end first

            char response[BUFFER_SIZE];
            memset(response, 0, BUFFER_SIZE);

            // Read execution output from pipe
            read(pipefd[0], response, BUFFER_SIZE - 1);
            close(pipefd[0]);

            // Handle error cases
            if (strlen(response) == 0) {
                snprintf(response, BUFFER_SIZE, "[ERROR] Command not found: \"%s\"\n", clientCommand);
                printf("[ERROR] Command not found: \"%s\"\n", clientCommand);
            } else {
                printf("[OUTPUT] Sending output to client:\n%s", response);
            }

            // Send response to client
            send(client_socket, response, strlen(response), 0);

            // Wait for child process to complete
            wait(NULL);
        }
    }

    // Close sockets
    close(client_socket);
    close(server_socket);

    return 0;
}
