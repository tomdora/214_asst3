#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <ctype.h>

#define BACKLOG 5

// the argument we will pass to the connection-handler threads
typedef struct Connection{
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
} Connection;

int server(char *port);
void * joke(void *arg);

int main(int argc, char *argv[]){
	if (argc != 2){
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
    (void) server(argv[1]);
    return EXIT_SUCCESS;
}

int server(char *port){
    struct addrinfo hint, *address_list, *addr;
    Connection *con;
    int error, sfd;
    pthread_t tid;
	
    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;
	
    error = getaddrinfo(NULL, port, &hint, &address_list);
    if (error != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }
	
    // attempt to create socket
    for(addr = address_list; addr != NULL; addr = addr->ai_next){
        sfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        
        // if we couldn't create the socket, try the next method
        if (sfd == -1){
            continue;
        }
		
        if ((bind(sfd, addr->ai_addr, addr->ai_addrlen) == 0) &&
            (listen(sfd, BACKLOG) == 0)){
            break;
        }
		
        // unable to set it up, so try the next method
        close(sfd);
    }

    if (addr == NULL){
        // we reached the end of result without successfuly binding a socket
        fprintf(stderr, "Could not bind\n");
        return -1;
    }
	
    freeaddrinfo(address_list);
	
    // at this point sfd is bound and listening
    printf("Waiting for connection\n");
    for(;;){
    	// create argument struct for child thread
		con = malloc(sizeof(Connection));
        con->addr_len = sizeof(struct sockaddr_storage);
		
        // wait for an incoming connection
        con->fd = accept(sfd, (struct sockaddr *) &con->addr, &con->addr_len);
        
        // if we got back -1, it means something went wrong
        if (con->fd == -1){
            perror("accept");
            continue;
        }
		
		// spin off a worker thread to handle the remote connection
        error = pthread_create(&tid, NULL, joke, con);
		
		// if we couldn't spin off the thread, clean up and wait for another connection
        if (error != 0){
            fprintf(stderr, "Unable to create thread: %d\n", error);
            close(con->fd);
            free(con);
            continue;
        }
		
		// otherwise, detach the thread and wait for the next connection request
        pthread_detach(tid);
    }
	
    // never reach here
    return 0;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



//Struct for taking the message and breaking it into its parts
typedef struct Message{
	char * type;
	int length;
	char * content;
} Message;

//Function to convert read() message to a Message struct
Message * convertInput(char * input, int i, Connection * con){
	Message * x = malloc(sizeof(Message));
	char * inputCopy = malloc(strlen(input) + 1);
	strcpy(inputCopy, input);
	char * split = strtok(inputCopy, "|");
	
	//Make sure that strtok split didn't return NULL; if it did, we don't have the right formatting
	if(split == NULL){
		char error[] = "ERR|MxFT|";
		error[5] = i + '0';
		printf("%d %s : first strtok() failed\n", i, error);
		
		//Write the error message to the client and return NULL
		write(con->fd, error, strlen(error));
		free(x);
		free(inputCopy);
		return NULL;
	}
	
	//First, we want to check to see if the type isn't "REG" (ERR is already handled)
	if(strcmp(split, "REG") != 0){
		char error[] = "ERR|MxFT|";
		error[5] = i + '0';
		printf("%d %s : not of type REG\n", i, error);
		
		//Write the error message to the client and return NULL
		write(con->fd, error, strlen(error));
		free(x);
		free(inputCopy);
		return NULL;
	}
	//If it's "REG" then we can copy to the struct
	x->type = malloc(strlen(split) + 1);
	strcpy(x->type, split);
	
	//strtok split for the next token
	split = strtok(NULL, "|");
	
	//Make sure that strtok split didn't return NULL; if it did, we don't have the right formatting
	if(split == NULL){
		char error[] = "ERR|MxFT|";
		error[5] = i + '0';
		printf("%d %s : second strtok() failed\n", i, error);
		
		//Write the error message to the client and return NULL
		write(con->fd, error, strlen(error));
		free(x);
		free(inputCopy);
		return NULL;
	}
	
	//Convert the length into a decimal and set the length
	x->length = atoi(split);
	//But check if atoi returned 0, which means it failed
	if(x->length == 0){
		char error[] = "ERR|MxFT|";
		error[5] = i + '0';
		printf("%d %s\n", i, error);
		
		//Write the error message to the client and return NULL
		write(con->fd, error, strlen(error));
		free(x->type);
		free(x);
		free(inputCopy);
		return NULL;
	}
	
	//strtok split for the next token
	split = strtok(NULL, "|");
	
	//Make sure that strtok split didn't return NULL; if it did, we don't have the right formatting
	if(split == NULL){
		char error[] = "ERR|MxFT|";
		error[5] = i + '0';
		printf("%d %s : third strtok() failed\n", i, error);
		
		//Write the error message to the client and return NULL
		write(con->fd, error, strlen(error));
		free(x);
		free(inputCopy);
		return NULL;
	}
	
	//Last, we can copy the content of the message
	x->content = malloc(strlen(split) + 1);
	strcpy(x->content, split);
	//And then check if the message length is the same as reported
	if(x->length != strlen(x->content)){
		char error[] = "ERR|MxLN|";
		error[5] = i + '0';
		printf("%d %s\n", i, error);
		
		//Write the error message to the client and return NULL
		write(con->fd, error, strlen(error));
		free(x->content);
		free(x->type);
		free(x);
		free(inputCopy);
		return NULL;
	}
	//Otherwise we're good to go
	free(inputCopy);
	return x;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



//Function to automate most of the read looping and checking
Message * readClient(Connection * c, char * port, char * expected, int nmessage){
	//Wait for "who's there" from client
	char buf[2];
	int nread;
	int pipeCounter = 0;
	//Start with 100 bytes for potential client input, but malloc it to allow for realloc. Keep an int to keep track of the size of the clientInput
	int inputSize = 100;
	char * clientInput = calloc(inputSize, 1);
	
	//Start the loop to read, checking for individual pipes along the way
	while(pipeCounter < 3 && 0 < (nread = read(c->fd, buf, 1))){
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
			close(c->fd);
			free(c);
			return NULL;
		}
        //printf("read %d bytes %s : %d pipes\n", nread, buf, pipeCounter);
    }
    //If the read gives some unknown error
    if(nread == -1){
		char error[] = "ERR|MxRE|";
		error[5] = nmessage + '0';
		printf("%s:%d		%s : read -1 error\n", port, nmessage, error);
		
		//Close the socket and return NULL
		close(c->fd);
		free(c);
		return NULL;
	}
    //Make sure that the message isn't too short for anything
	if(strlen(clientInput) < 7){
		char error[] = "ERR|MxFT|";
		error[5] = nmessage + '0';
		printf("%s:%d		%s : message too short\n", port, nmessage, error);
		write(c->fd, error, strlen(error));
		
		//Close the socket and return NULL
		close(c->fd);
		free(c);
		return NULL;
	}
	printf("%s received.\n", clientInput);
	//Split the input into tokens and check if it's valid
	Message * convertedInput = convertInput(clientInput, nmessage, c);
	if(convertedInput == NULL){
		close(c->fd);
		free(c);
		return NULL;
	}
	//Make sure it's the expected response; if expected is NULL, we don't expect any specific response.
	else if(expected != NULL && strcmp(convertedInput->content, expected) != 0){
		char error[] = "ERR|MxCT|";
		error[5] = nmessage + '0';
		printf("%s:%d		%s\n", port, nmessage, error);
		write(c->fd, error, strlen(error));
		
		//Close the socket and return NULL
		close(c->fd);
		free(convertedInput->type);
		free(convertedInput->content);
		free(convertedInput);
		free(c);
		return NULL;
	}
	
	//If we get to this point, everything went well! Time to return the Message * convertedInput.
	return convertedInput;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



void * joke(void * arg){
    char host[100], port[10], buf[2];
    Connection * c = (Connection *) arg;
    int error, nread, pipeCounter, nmessage;
	
	// find out the name and port of the remote host
    error = getnameinfo((struct sockaddr *) &c->addr, c->addr_len, host, 100, port, 10, NI_NUMERICSERV);
    	// we provide:
    	// the address and its length
    	// a buffer to write the host name, and its length
    	// a buffer to write the port (as a string), and its length
    	// flags, in this case saying that we want the port as a number, not a service name
    if (error != 0){
        fprintf(stderr, "getnameinfo: %s", gai_strerror(error));
        close(c->fd);
        return NULL;
    }
	
	//Code to implement a timeout if so desired
	/*struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);*/
	
    printf("\n[%s:%s] connection\n", host, port);
	
	
	
	//Send "knock knock" to client
	nmessage = 0;
	char knock[] = "REG|13|Knock, knock.|";
	write(c->fd, knock, strlen(knock));
	printf("%s:%d		Knock, knock.\n", port, nmessage);
	
	
	
	//Listen for "Who's there?" from client
	nmessage = 1;
	Message * whosThere = readClient(c, port, "Who's there?", nmessage);
	if(whosThere == NULL) return NULL;
	printf("%s:%d				%s\n", port, nmessage, whosThere->content);
	
	
	
	//Send "who" response
	nmessage = 2;
	char response[] = "REG|3|Ya.|";
	write(c->fd, response, strlen(response));
	printf("%s:%d		Ya.\n", port, nmessage);
	
	
	
	//Listen for "Ya, who?" from client
	nmessage = 3;
	Message * whoWho = readClient(c, port, "Ya, who?", nmessage);
	if(whoWho == NULL) return NULL;
	printf("%s:%d				%s\n", port, nmessage, whoWho->content);
	
	
	
	//Send the punchline to the client
	nmessage = 4;
	char punch[] = "REG|46|No thanks, I don't like having my data stolen.|";
	write(c->fd, punch, strlen(punch));
	printf("%s:%d		No thanks, I don't like having my data stolen.\n", port, nmessage);
	
	
	
	//Listen for disgust from client
	nmessage = 5;
	Message * disgust = readClient(c, port, NULL, nmessage);
	if(disgust == NULL) return NULL;
	printf("%s:%d				%s\n", port, nmessage, disgust->content);
	
	
	
    close(c->fd);
    free(c);
    return NULL;
}
