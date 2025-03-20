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

#define PORT 8081
#define BUFFER_SIZE 2048

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address;
    int opt = 1;
    char clientCommand[BUFFER_SIZE];
    char *parsedCommand[50];
    int argCount;

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("[ERROR] Socket creation failed");
        exit(1);
    }

    // Allow address reuse
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[ERROR] setsockopt failed");
        exit(1);
    }

    // Configure server address
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("[ERROR] Socket bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for client connections
    if (listen(server_socket, 5) < 0) {
        perror("[ERROR] Socket listen failed");
        exit(EXIT_FAILURE);
    }
    printf("[INFO] Server started, waiting for client connections...\n");

    // Accept client connection
    int addrlen = sizeof(server_address);
    client_socket = accept(server_socket, (struct sockaddr*)&server_address, (socklen_t*)&addrlen);
    if (client_socket < 0) {
        perror("[ERROR] Socket accept failed");
        exit(EXIT_FAILURE);
    }
    printf("[INFO] Client connected.\n");

    //Main loop to handle client commands
    while (1) {
        memset(clientCommand, 0, sizeof(clientCommand));
        int bytesReceived = recv(client_socket, clientCommand, sizeof(clientCommand), 0);

        if (bytesReceived <= 0) {
            printf("[INFO] Client disconnected.\n");
            break;
        }

        // Print received command for debugging
        printf("[RECEIVED] Command: \"%s\" from client.\n", clientCommand);

        // Handle exit command
        if (strcmp(clientCommand, "exit") == 0) {
            printf("[INFO] Closing connection.\n");
            break;
        }

        // Parse the command
        parseInput(clientCommand, parsedCommand, &argCount);

        // If user just pressed Enter, continue loop
        if (parsedCommand[0] == NULL) {
            continue;
        }

        //Determine command type (pipes, redirections, etc.)
        int pipeFound = 0, redirectFound = 0;
        int inputRedirectFound = 0, outputRedirectFound = 0, errorRedirectFound = 0;

        for (int j = 0; j < argCount; j++) {
            if (strcmp(parsedCommand[j], "|") == 0) {
                pipeFound = 1;
            }
            if (strcmp(parsedCommand[j], "<") == 0) {
                redirectFound = 1;
                inputRedirectFound = 1;
            }
            if (strcmp(parsedCommand[j], ">") == 0) {
                redirectFound = 1;
                outputRedirectFound = 1;
            }
            if (strcmp(parsedCommand[j], "2>") == 0 || strcmp(parsedCommand[j], "2>&1") == 0) {
                redirectFound = 1;
                errorRedirectFound = 1;
            }
        }

        //Create a pipe to capture command output
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("[ERROR] Pipe creation failed");
            continue;
        }

        printf("[EXECUTING] Executing command: \"%s\"\n", clientCommand);

        pid_t pid = fork();
        if (pid == 0) {  //Child process
            close(pipefd[0]);  
            dup2(pipefd[1], STDOUT_FILENO);  
            dup2(pipefd[1], STDERR_FILENO);  
            close(pipefd[1]);

            //Select the appropriate handler based on command type
            if (pipeFound && redirectFound) {
                handlePipeRedirect(parsedCommand);
            }
            else if (pipeFound) {
                int pipeCount = 0;
                for (int j = 0; j < argCount; j++) {
                    if (strcmp(parsedCommand[j], "|") == 0) {
                        pipeCount++;
                    }
                }
                
                if (pipeCount > 1) {
                    handleMultiplePipes(parsedCommand);
                }
                else {
                    handlePipes(parsedCommand);
                }
            }
            else if (redirectFound) {
                if ((inputRedirectFound && outputRedirectFound) || 
                    (inputRedirectFound && errorRedirectFound) || 
                    (outputRedirectFound && errorRedirectFound)) {
                    handleCombinedRedirect(parsedCommand);
                }
                else if (inputRedirectFound) {
                    inputRedirect(parsedCommand);
                }
                else {
                    handleRedirect(parsedCommand);
                }
            }
            else {
                noArgCommand(parsedCommand);
            }

            exit(0);
        } 
        else {  
            close(pipefd[1]);  // Close write end

            char response[BUFFER_SIZE];
            memset(response, 0, BUFFER_SIZE);

            // Read execution output from pipe
            ssize_t bytesRead = read(pipefd[0], response, BUFFER_SIZE - 1);
            close(pipefd[0]);  // Close read end

            int status;
            waitpid(pid, &status, 0);  // Ensure child process is properly cleaned up

            // Ensure response is null-terminated
            if (bytesRead > 0) {
                response[bytesRead] = '\0';  // Terminate string correctly
            } else {
                response[0] = '\0';  // If no output, keep response empty
            }

            // **Ensure something is always sent to prevent client hang**
            if (strlen(response) == 0) {
                strcpy(response, "\n");  // Send a newline instead of empty string
            }

            // Send response to client
            send(client_socket, response, strlen(response), 0);
        }

    }

    // Close sockets
    close(client_socket);
    close(server_socket);

    return 0;
}
