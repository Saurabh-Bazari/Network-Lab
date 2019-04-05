#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <time.h>

struct user_info{				// Store Current userID and its type
    char username[100];
    char type;
};

void send_msg_client(int sock_fd, char *str) 				// For Send message to client
{
	int size = strlen(str);
    write(sock_fd, &size, sizeof(int));						// send size of message first
    write(sock_fd, str, strlen(str)*sizeof(char));			// send entire message 
}

char* recv_msg_client(int sock_fd) 							// For receive message from Client
{
	int size = 0; 
    read(sock_fd, &size, sizeof(int));						// receive size of message first 
    char * str_p = (char*)malloc(size*sizeof(char));		// then allocate that size of space
    read(sock_fd, str_p, size*sizeof(char));				// receive entire message from clent
    str_p[size] = '\0';
	if(strcmp(str_p, "exit")==0){							// If received message is "exit" then terminate
		exit(EXIT_SUCCESS);
	}
	return str_p;											// return received message
}
char * getfield(char* line, int num)						// extract (num)th field from line which seperated by ,
{ 
    char *token = strtok(line, ","); 						
    while (token != NULL) 
    { 
		if(!--num){
        	return token;
		} 
        token = strtok(NULL, ","); 
    } 
}

struct user_info Auth(int client){									// return user info after authorization of client  
	struct user_info user;
	while(1){
		char * username, * password;								// received UserId and password
		send_msg_client(client, "Enter Username");
		username = recv_msg_client(client);

		send_msg_client(client, "Enter Password");
		password = recv_msg_client(client);
				
		struct user_info user;
		FILE * login;
		login = fopen("login.txt", "r");						// open login.txt file for check whether current user is authorized
		char line[1000];
		while(fgets(line, 1000, login)){
			char * col1, *col2, *col3;							// col1, 2, 3 correspond field format of login file (username, password, type)
			char * line1 = strdup(line);
			char * line2 = strdup(line);
			col1 = getfield(line1, 1);
			col2 = getfield(line2, 2);
			col3 = getfield(line, 3);
			if(strcmp(col1, username)==0 && strcmp(col2, password)==0){					// Verify Current Client is Authorized or not from login.txt file
				strcpy(user.username,username);
				user.type = col3[0];
				return user;
			}
		}
		send_msg_client(client, "Invalid Credentials");			
		fclose(login);
	}
    return user;
}

char * get_balance(FILE * user_file){							// for a given userfile, find the latest balance which is found in the last row
	char *line = (char*)malloc(1000*sizeof(char));
	char * last_line = (char*)malloc(1000*sizeof(char));
	while(fgets(line, 1000, user_file)){
		strcpy(last_line, line);
	}
	char * balance;
	balance = getfield(last_line, 3);
	if (!balance)
		balance = "0";
	return balance;												// The final balance
}
int count=0;
char * get_mini_stmt(FILE * user_file){							// function to get the bottom 10 (latest) entries of the account statement file.

	char *line = (char*)malloc(10000*sizeof(char));
	char * stmt = (char*)malloc(10000*sizeof(char));

	if (fgets(stmt, 1000, user_file)==0 )
	{
		count = 10;
		return stmt ;
	}

	line = get_mini_stmt(user_file);

	if (count>0)
	{
		strcat(line, stmt);
		count--;
	}

	return line;
}

void client_user(int client, struct user_info user){			// This function correspond to the instance when the client(user) is the customer. 
	char * response;
	send_msg_client(client, "Press 1 to view Main Balance\nPress 2 to view Mini Statement");
	free(response);
	response = recv_msg_client(client);
	FILE * user_file;
	user_file = fopen(user.username, "r");
	while(1){													// The server sends various options to the customer to choose
		fseek(user_file, 0, SEEK_SET);
		if(strcmp(response, "1")==0){
			char * balance = get_balance(user_file);			// Call function to get the account balance providing the file pointer. 
			// send_msg_client(client, balance);
			char msg[1000] = "Current Account Balance: ";
			strcat(msg, balance);
			strcat(msg, "\nPress 1 to view Main Balance\nPress 2 to view Mini Statement:");
			send_msg_client(client, msg);
			free(response);
			response = recv_msg_client(client);
		}else if(strcmp(response, "2")==0){
			char * stmt = get_mini_stmt(user_file);				// Call function to get the mini statement providing the file pointer.
			char msg[10000] = "Mini Statement:\n";
			strcat(msg, stmt);
			strcat(msg, "\nPress 1 to view Main Balance\nPress 2 to view Mini Statement:");
			send_msg_client(client, msg);
			free(response);
			response = recv_msg_client(client);
		}else{													// Condition when client sends incorrect response.
			send_msg_client(client, "Please Enter a Valid Response");
			free(response);
			response = recv_msg_client(client);
		}
	}
}

void client_police(int client, struct user_info user){			// This function correspond to the instance when the client(user) is the police.
	char * response;
	send_msg_client(client, "Enter Customer ID");				// Ask the id of the customer from the police.
	free(response);
	response = recv_msg_client(client);
	FILE * login;
	login = fopen("login.txt", "r");
	while(1){													// Search for the given customer.
		fseek(login, 0, SEEK_SET);
		char *line = (char*)malloc(1000*sizeof(char));
		int flag = 0;
		while(fgets(line, 1000, login)){				
			char * col1, * col3;
			char * line1 = strdup(line);
			col1 = getfield(line1, 1);
			col3 = getfield(line, 3);
			if(strcmp(response, col1)==0 && strcmp(col3, "C")==0){	// When found, open his bank file and print the balance and mini statement.
				FILE * user_file;
				user_file = fopen(col1, "r");
				char * balance = get_balance(user_file);
				fseek(user_file, 0, SEEK_SET);
				char * mini_stmt = get_mini_stmt(user_file);
				char msg[10000] = "Current Balance: ";
				strcat(msg, balance);
				strcat(msg, "\nMini Statement: \n");
				strcat(msg, mini_stmt);
				strcat(msg, "\nEnter Customer ID");
				send_msg_client(client, msg);
				flag = 1;
				break;
			}
		}
		if(!flag){
			char * m = "Invalid Customer ID\nEnter Customer ID";
			send_msg_client(client, m);
		}
		free(response);
		response = recv_msg_client(client);
	}
}

void client_admin(int client, struct user_info user){	// This function correspond to the instance when the client(user) is the customer.
	
	char * response;
	send_msg_client(client, "Enter Customer ID");		//Ask admin for the customer ID
	free(response);
	response = recv_msg_client(client);
	FILE * login;
	login = fopen("login.txt", "r");
	while(1){
		fseek(login, 0, SEEK_SET);
		char *line = (char*)malloc(1000*sizeof(char));
		int flag = 0;
		while(fgets(line, 1000, login)){				// Search for the customer
			char * col1, * col3;
			char * line1 = strdup(line);
			col1 = getfield(line1, 1);
			col3 = getfield(line, 3);
			if(strcmp(response, col1)==0 && strcmp(col3, "C")==0){		// When found, give admin folowing options
				flag = 1;
				send_msg_client(client, "Choose transaction type:\n1) Credit\n2) Debit\n3) Choose Another Customer  ");	
				while(1){
					FILE * user_file;
					user_file = fopen(col1, "r");
					char * balance = get_balance(user_file);
					fseek(user_file, 0, SEEK_SET);
					free(response);
					response = recv_msg_client(client);
					fclose(user_file);
					if(strcmp(response, "3")==0){
						send_msg_client(client, "Enter Customer ID");
						break;
					}
					else if(strcmp(response, "1")==0){
						send_msg_client(client, "Enter the Amount:");
						free(response);
						response = recv_msg_client(client);

						if (atoi(response) <= 0)
						{
							send_msg_client(client, "Invalid Amount\nChoose transaction type:\n1) Credit\n2) Debit\n3) Choose Another Customer  ");	
							continue;
						}

						char * new_balance = (char*)malloc(10*sizeof(char));
						char * current_date = (char*)malloc(12*sizeof(char));
						//Current Date/Time
						time_t t = time(NULL);
						struct tm tm = *localtime(&t);
						sprintf(current_date, "%d/%d/%d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
						//Date time end
						sprintf(new_balance, "%d", atoi(balance) + atoi(response));
						char msg[10000];									// msg is to be the new entry in the customer file
						strcat(msg, current_date);							// It contains today's date, transaction type, and final balance
						strcat(msg, ",");
						strcat(msg, "Credit");
						strcat(msg, ",");
						strcat(msg, new_balance);
						strcat(msg, ",\n");
						user_file = fopen(col1, "a");
						fprintf(user_file, "%s", msg);
						msg[0]= '\0';
						fclose(user_file);
						send_msg_client(client, "Updated Successfully\nChoose transaction type:\n1) Credit\n2) Debit\n3) Choose Another Customer  ");
					}
					else if(strcmp(response, "2")==0){
						send_msg_client(client, "Enter the Amount:");
						free(response);
						response = recv_msg_client(client);
						if (atoi(response) <= 0)
						{
							send_msg_client(client, "Invalid Amount\nChoose transaction type:\n1) Credit\n2) Debit\n3) Choose Another Customer  ");	
							continue;
						}
						char * new_balance = (char*)malloc(10*sizeof(char));
						char * current_date = (char*)malloc(12*sizeof(char));
						//Current Date/Time
						time_t t = time(NULL);
						struct tm tm = *localtime(&t);
						sprintf(current_date, "%d/%d/%d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
						//Date time end
						if(atoi(balance) >= atoi(response)){
							char * new_balance = (char*)malloc(10*sizeof(char));
							sprintf(new_balance, "%d", atoi(balance) - atoi(response));
							char msg[10000];								// msg is to be the new entry in the customer file
							strcat(msg, current_date);						// It contains today's date, transaction type, and final balance
							strcat(msg, ",");
							strcat(msg, "Debit");
							strcat(msg, ",");
							strcat(msg, new_balance);
							strcat(msg, ",\n");
							user_file = fopen(col1, "a");
							fprintf(user_file, "%s", msg);
							msg[0]= '\0';
							fclose(user_file);
							send_msg_client(client, "Updated Successfully\nChoose transaction type:\n1) Credit\n2) Debit\n3) Choose Another Customer  ");
						}
						else{											// This condition when the intended debit amount is more than the balance 
							send_msg_client(client, "Not Sufficient Balance\nChoose transaction type:\n1) Credit\n2) Debit\n3) Choose Another Customer  ");
						}
					}
					else{												// When the admin chooses none of the given options
						send_msg_client(client, "Invalid Option.\nChoose transaction type:\n1) Credit\n2) Debit\n3) Choose Another Customer  ");

					}
				}
				break;
			}
		}
		if(!flag){												// When the given customer is not found in login.txt file or his type is not 'customer'
			send_msg_client(client, "Invalid Customer ID. \nEnter Customer ID ");
		}
		free(response);
		response = recv_msg_client(client);
	}
}

int main(int argc, char const *argv[]){
    int serverPort = atoi(argv[1]);								// The port of the server is taken from command line
    int server_fd, new_socket; 									// server_fd is the socket of server, new_socket is that of client.
    struct sockaddr_in address, client_addr; 
    int addrlen = sizeof(client_addr); 
    char buffer[1024] = {0}; 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 		// When the socket creation was not successful 
    { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    }
    memset((void*)&address, 0, sizeof(address));
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( serverPort ); 
    
    // Forcefully attaching socket to the given port 
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    // When the socket is successfully created, listen(wait) for initiation from a client.
    if (listen(server_fd, 7) < 0) 
    { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    } 
    
    // In below section a new thread is created for a client and terminated when 'exit' is called
    while(1){
        memset(&client_addr, 0, sizeof(client_addr));
        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen))<0) 
        { 
            perror("accept"); 
            exit(EXIT_FAILURE); 
        }
        switch(fork()) 
		{
			case -1:
				printf("Error During forking a new child process :- \n");
				break;
			case 0:
				close(server_fd);
                struct user_info user;
                user = Auth(new_socket);						// Initiate the message exchange corresponding to authorization of the user.
				if(user.type=='C'){								// Type 'C' is for the regular customer
					client_user(new_socket, user);				// Start customer message exchange.
				}else if(user.type=='P'){						// Type 'P' is for the police
					client_police(new_socket, user);			// Start Police message exchange.
				}else if(user.type=='A'){						// Type 'A' is for the bank admin
					client_admin(new_socket, user);				// Start Admin message exchange.
				}
				exit(EXIT_SUCCESS);
				break;
			default:
				close(new_socket);
				break;
		}
        
    }
    return 0; 
}