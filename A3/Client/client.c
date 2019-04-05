#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
   
void send_msg_server(int sock_fd, char *str) 			// To send the string 'str' to the server
{
    int size = strlen(str);								// Size of the str message
    write(sock_fd, &size, sizeof(int));
    write(sock_fd, str, strlen(str)*sizeof(char));
}

char* get_msg_server(int sock_fd) 						// To recieve the message from the server and return in form of string.
{							
	int size = 0; 
    read(sock_fd, &size, sizeof(int));					// Size of message is received first
    char * str_p = (char*)malloc(size*sizeof(char));
    read(sock_fd, str_p, size*sizeof(char));			// The entire message is received using the size.
    str_p[size] = '\0';
	return str_p;
}

int main(int argc, char const *argv[]) 
{ 
    int serverPort = atoi(argv[2]);			// Port number of the server.
    char * serverIP = argv[1];				// IP of the server.
    struct sockaddr_in address; 			
    int sock = 0; 
    char buffer[1024] = {0}; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) // creates a socket in the specified domain and of the specified type.
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   
    memset(&address, 0, sizeof(address)); 
   
    address.sin_family = AF_INET; 			// AF_INET for socket programming over a network.
    address.sin_addr.s_addr= inet_addr(serverIP);
    address.sin_port = htons(serverPort); 	// This function makes sure that numbers are stored in memory in network byte order, which is with the most significant byte first.
   
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) < 0) 	// Attemp to connect to the server.
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 

    // This part of code is only executed on successful TCP connection with the server.

    printf(" -----------------------:- Welcome to Deutche Bank :-------------------------\n");
    char * msg_from_server;			// Variable that stores message received from the server.
    char * msg_to_server;			// Variable that stores message to be sent to the server.
    
    while(1)																// continue interaction with server till "exit" scanned
	{
		msg_from_server = get_msg_server(sock);   							// received message from Server

		// if(msg_from_server == NULL || strcmp(msg_from_server, "")==0){		
  //           break;
  //       }
        if(strcmp(msg_from_server, "Invalid Credentials")==0){				// if invalid Credentials then again received message from server
            printf("Invalid Credentials\n");
            msg_from_server = get_msg_server(sock);
        }
        printf("%s (enter exit to Terminate) \n",msg_from_server);			// message to client which received from server
		msg_to_server[0] = '\0';		
		scanf("%s", msg_to_server);											// read input from server
	    send_msg_server(sock, msg_to_server);								// send scanned message to server
		
		if(strcmp(msg_to_server, "exit") == 0) 								// if scanned message is exit then terminate
		{
			shutdown(sock, SHUT_WR);
			break;
		}
        printf("\n-----------------------------------------------------------------------\n");
    
    }
    return 0; 
} 