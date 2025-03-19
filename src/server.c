#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>

#define PORT 8080

int main(){
    // Create a socket
    int server_socket;
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    //check for fail error
    if (server_socket == -1){
        printf("Socket creation failed\n");
        exit(1);
    }

    //define server address structure
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;


	//bind the socket to our specified IP and port
	if (bind(server_socket , 
		(struct sockaddr *) &server_address,
		sizeof(server_address)) < 0)
	{
		printf("socket bind failed..\n");
        exit(EXIT_FAILURE);
	}
	 

	//listen for connections
    if (listen(server_socket, 5) < 0)
    {
        printf("socket listen failed..\n");
        exit(EXIT_FAILURE);
    }

    int addrlen = sizeof(server_address);

    //accept the connection
    int client_socket;
    client_socket = accept(server_socket, 
        (struct sockaddr *) &server_address,
        (socklen_t*)&addrlen);

    if (client_socket < 0)
    {
        printf("socket accept failed..\n");
        exit(EXIT_FAILURE);
    }
}