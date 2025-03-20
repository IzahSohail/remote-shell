#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8081

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char userInput[100];
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
        printf("$ ");
        fflush(stdout);
        fgets(userInput, sizeof(userInput), stdin);
        userInput[strcspn(userInput, "\n")] = 0;  // Remove newline character

        // Check if input is empty (just pressing "Enter")
        if (strlen(userInput) == 0) {
            continue;  // Skip sending empty commands
        }

        // Send command to server
        if (send(sock, userInput, strlen(userInput), 0) == -1) {
            perror("Send failed");
            break;
        }

        // Exit client when user types exit
        if (strcmp(userInput, "exit") == 0) {
            printf("Exiting client.\n");
            break;
        }

        // Receive server response
        memset(serverResponse, 0, sizeof(serverResponse));
        ssize_t bytesReceived = recv(sock, serverResponse, sizeof(serverResponse) - 1, 0);

        //Properly handle server disconnection
        if (bytesReceived == -1) {
            perror("Receive failed");
            break;
        } else if (bytesReceived == 0) {
            printf("[INFO] Server closed the connection.\n");
            break;
        }

        serverResponse[bytesReceived] = '\0';  // Ensure null termination

        //If response is empty, just return to prompt
        if (strlen(serverResponse) == 0 || strcmp(serverResponse, "\n") == 0) {
            continue;  // Prevents infinite hang
        }

        // Print server response
        printf("%s", serverResponse);
    }

    // Close socket before exiting
    close(sock);
    return 0;
}
