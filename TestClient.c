#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <ctype.h>

//Struct for taking the message and breaking it into its parts
typedef struct Message{
	char * type;
	int length;
	char * content;
} Message;

//Function to convert read() message to a Message struct
Message * convertInput(char * input, int i, int sock){
	Message * x = malloc(sizeof(Message));
	char * split = strtok(input, "|");
	
	printf("Converting.\n");
	
	//First, we want to check if the message is an ERR and return NULL if so
	if(strcmp(split, "ERR") == 0){
		printf("%d %s\n", i, input);
		free(x);
		return NULL;
	} //Next we check to see if the type isn't "REG"
	else if(strcmp(split, "REG") != 0){
		char error[] = "ERR|MxFT|";
		error[5] = i + '0';
		printf("%s\n", error);
		
		//Write the error message to the client and return NULL
		write(sock, error, strlen(error));
		free(x);
		return NULL;
	}
	//If it's "REG" then we can copy to the struct
	x->type = malloc(strlen(split) + 1);
	strcpy(x->type, split);
	
	//Convert the length into a decimal and set the length
	split = strtok(NULL, "|");
	x->length = atoi(split);
	//But check if atoi returned 0, which means it failed
	if(x->length == 0){
		char error[] = "ERR|MxLN|";
		error[5] = i + '0';
		printf("%s\n", error);
		
		//Write the error message to the client and return NULL
		write(sock, error, strlen(error));
		free(x->type);
		free(x);
		return NULL;
	}
	
	//Check for punctuation from the server
	
	//Last, we can copy the content of the message
	split = strtok(NULL, "|");
	if(ispunct(split[strlen(split) - 1]) != 0){
		x->content = malloc(strlen(split) + 1);
		strcpy(x->content, split);
	}
	else{
		//Else it's a format error
		char error[] = "ERR|MxFT|";
		error[5] = i + '0';
		printf("%s\n", error);
		
		//Write the error message to the client and return NULL
		write(sock, error, strlen(error));
		free(x->content);
		free(x->type);
		free(x);
		return NULL;
	}
	//And then check if the message length is the same as reported
	if(x->length != strlen(x->content)){
		char error[] = "ERR|MxLN|";
		error[5] = i + '0';
		printf("%s\n", error);
		
		//Write the error message to the client and return NULL
		write(sock, error, strlen(error));
		free(x->content);
		free(x->type);
		free(x);
		return NULL;
	}
	//Otherwise we're good to go
	return x;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



//Function to automate most of the read looping and checking
Message * readServer(int sock, char * port, char * expected, int nmessage){
	//Wait for "who's there" from client
	char buf[2];
	int nread;
	int pipeCounter = 0;
	//Start with 100 bytes for potential client input, but malloc it to allow for realloc. Keep an int to keep track of the size of the clientInput
	int inputSize = 100;
	char * clientInput = calloc(inputSize, 1);
	
	//Start the loop to read, checking for individual pipes along the way
	while(pipeCounter < 3 && 0 < (nread = read(sock, buf, 1))){
        buf[nread] = '\0';
        if(buf[0] == '|') pipeCounter++;
		
		//Make sure we don't need to grow our clientInput string; if we do, grow it by 1.5x each time.
		if(strlen(clientInput) == inputSize-1){
			inputSize = inputSize * 1.5;
			clientInput = realloc(clientInput, inputSize);
		}
		//Add the byte to the end of the string and keep going
		strcat(clientInput, buf);
		
		//An ERR message only has two '|', so we need to explicitly check for an error message
		//We don't check for specific errors, or even if the error's contents are correct (only the length), we just decide that an ERR is enough to close.
		if(strlen(clientInput) == 9 && strncmp(clientInput, "ERR", 3) == 0){
			printf("%s:%d		%s\n", port, nmessage, clientInput);
			close(sock);
			return NULL;
		}
        //printf("read %d bytes %s : %d pipes\n", nread, buf, pipeCounter);
    }
    //If the read gives some unknown error
    if(nread == -1){
		char error[] = "ERR|Mx??|";
		error[5] = nmessage + '0';
		printf("%s:%d		%s : read -1 error\n", port, nmessage, error);
		
		//Close the socket and return NULL
		close(sock);
		return NULL;
	}
    //Make sure that the message isn't too short for anything
	if(strlen(clientInput) < 7){
		char error[] = "ERR|MxFT|";
		error[5] = nmessage + '0';
		printf("%s:%d		%s : message too short\n", port, nmessage, error);
		write(sock, error, strlen(error));
		
		//Close the socket and return NULL
		close(sock);
		return NULL;
	}
	printf("%s received.\n", clientInput);
	//Split the input into tokens and check if it's valid
	Message * convertedInput = convertInput(clientInput, 1, sock);
	if(convertedInput == NULL){
		close(sock);
		return NULL;
	}
	//Make sure it's the expected response; if expected is NULL, we don't expect any specific response.
	else if(expected != NULL && strcmp(convertedInput->content, expected) != 0){
		char error[] = "ERR|MxCT|";
		error[5] = nmessage + '0';
		printf("%s:%d		%s\n", port, nmessage, error);
		write(sock, error, strlen(error));
		
		//Close the socket and return NULL
		close(sock);
		return NULL;
	}
	
	//If we get to this point, everything went well! Time to return the Message * convertedInput.
	return convertedInput;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void clientCorrect(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error, sock, i, nmessage;

	// we need to provide some additional information to getaddrinfo using hints
	// we don't know how big hints is, so we use memset to zero out all the fields
	memset(&hints, 0, sizeof(hints));
	
	// indicate that we want any kind of address
	// in practice, this means we are fine with IPv4 and IPv6 addresses
	hints.ai_family = AF_UNSPEC;
	
	// we want a socket with read/write streams, rather than datagrams
	hints.ai_socktype = SOCK_STREAM;

	// get a list of all possible ways to connect to the host
	// argv[1] - the remote host
	// argv[2] - the service (by name, or a number given as a decimal string)
	// hints   - our additional requirements
	// address_list - the list of results
	
	char * port = argv[2];
	
	error = getaddrinfo(argv[1], argv[2], &hints, &address_list);
	if (error) {
		fprintf(stderr, "%s", gai_strerror(error));
		exit(EXIT_FAILURE);
	}

	
	// try each of the possible connection methods until we succeed
	for (addr = address_list; addr != NULL; addr = addr->ai_next) {
		// attempt to create the socket
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		
		// if we somehow failed, try the next method
		if (sock < 0) continue;
		
		// try to connect to the remote host using the socket
		if (connect(sock, addr->ai_addr, addr->ai_addrlen) == 0) {
			// we succeeded, so break out of the loop
			break;
		}
		
		// we weren't able to connect; close the socket and try the next method		
		close(sock);
	}
	
	// if we exited the loop without opening a socket and connecting, halt
	if (addr == NULL) {
		fprintf(stderr, "Could not connect to %s:%s\n", argv[1], argv[2]);
		exit(EXIT_FAILURE);
	}
	
	// now that we have connected, we don't need the addressinfo list, so free it
	freeaddrinfo(address_list);
	
	
	
	//Listen for "Knock, knock." from server
	nmessage = 0;
	Message * knockKnock = readServer(sock, port, "Knock, knock.", nmessage);
	if(knockKnock == NULL) return;
	printf("%s:%d				%s|%d|%s|\n", port, nmessage, knockKnock->type, knockKnock->length, knockKnock->content);
	
	
	
	//Send "who's there" response to the server
	nmessage = 1;
	char whoThere[] = "REG|12|Who's there?|";
	write(sock, "ERR|M0LN|", strlen(whoThere));
	
	
	
	//Listen the setup
	nmessage = 2;
	Message * setup = readServer(sock, port, NULL, nmessage);
	if(setup == NULL) return;
	printf("%s:%d				%s|%d|%s|\n", port, nmessage, setup->type, setup->length, setup->content);
	
	
	
	//Create the "response who?" message
	nmessage = 3;
	char whoLength[10];
	sprintf(whoLength, "%ld", strlen(setup->content) + 5);
	char * whoWho = malloc(strlen(setup->content) + strlen(whoLength) + 12);
	
	strcpy(whoWho, "REG|");
	strcat(whoWho, whoLength);
	strcat(whoWho, "|");
	strncat(whoWho, setup->content, strlen(setup->content)-1);
	strcat(whoWho, ", who?|");
	
	printf("%s\n", whoWho);
	//Send the "response who?" message
	write(sock, whoWho, strlen(whoWho));
	
	
	
	//Listen for the punchline
	nmessage = 4;
	Message * punchLine = readServer(sock, port, NULL, nmessage);
	if(punchLine == NULL) return;
	printf("%s:%d				%s|%d|%s|\n", port, nmessage, punchLine->type, punchLine->length, punchLine->content);
	
	
	
	//React in disgust
	nmessage = 5;
	char react[] = "REG|4|Ugh.|";
	write(sock, react, strlen(react));
	
	
	
	// close the socket
	close(sock);

	return;	
}

int main(int argc, char * argv[]){
	
	if (argc < 3) {
		printf("Usage: %s [host] [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	clientCorrect(argv);
	
	return EXIT_SUCCESS;
}
