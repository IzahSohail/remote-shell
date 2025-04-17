#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8081
#define BUFFER_SIZE 4096

#define MAX_LINE 8192  // handle long input lines

char* readFullLine() {
    char *input = NULL;
    size_t len = 0;
    ssize_t read;

    printf("$ ");
    fflush(stdout);

    read = getline(&input, &len, stdin);
    if (read == -1) {
        free(input);
        return NULL;
    }

    // remove newline if present
    if (input[read - 1] == '\n') {
        input[read - 1] = '\0';
    }

    return input;
}

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    // char userInput[100];
    char serverResponse[1024];

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Define server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);  // localhost, can be modified to any IP address

    // Connect to the server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Connection to server failed");
        exit(1);
    }

    printf("Connected to server.\n");

    while (1) {
        char *userInput = readFullLine();
        if (!userInput) break;

        if (strlen(userInput) == 0) {
            free(userInput);
            continue;
        }

        if (send(sock, userInput, strlen(userInput), 0) == -1) {
            perror("Send failed");
            free(userInput);
            break;
        }

        if (strcmp(userInput, "exit") == 0) {
            printf("Exiting client.\n");
            free(userInput);
            break;
        }

        free(userInput);

        // Receive server response
       memset(serverResponse, 0, sizeof(serverResponse));
       ssize_t bytesReceived;

        while ((bytesReceived = recv(sock, serverResponse, sizeof(serverResponse) - 1, 0)) > 0) {
            serverResponse[bytesReceived] = '\0';  // Ensure null termination
            printf("%s", serverResponse);  // Print received chunk immediately

            // If we received fewer bytes than buffer size, that means there's no more data
            if (bytesReceived < sizeof(serverResponse) - 1) {
                break;
            }
        }

        //Properly handle server disconnection
        if (bytesReceived == -1) {
            perror("Receive failed");
            break;
        } else if (bytesReceived == 0) {
            printf("[INFO] Server closed the connection.\n");
            break;
        }

        serverResponse[bytesReceived] = '\0';  // Ensure null termination
    }

    // Close socket before exiting
    close(sock);
    return 0;
}
