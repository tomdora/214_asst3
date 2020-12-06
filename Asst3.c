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
	// setting AI_PASSIVE means that we want to create a listening socket
	
    // get socket and address info for listening port
    // - for a listening socket, give NULL as the host name (because the socket is on
    //   the local host)
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
		
        // if we were able to create the socket, try to set it up for
        // incoming connections;
        // 
        // note that this requires two steps:
        // - bind associates the socket with the specified port on the local host
        // - listen sets up a queue for incoming connections and allows us to use accept
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
        	// addr_len is a read/write parameter to accept
        	// we set the initial value, saying how much space is available
        	// after the call to accept, this field will contain the actual address length
		
        // wait for an incoming connection
        con->fd = accept(sfd, (struct sockaddr *) &con->addr, &con->addr_len);
        	// we provide
        	// sfd - the listening socket
        	// &con->addr - a location to write the address of the remote host
        	// &con->addr_len - a location to write the length of the address
        	//
        	// accept will block until a remote host tries to connect
        	// it returns a new socket that can be used to communicate with the remote
        	// host, and writes the address of the remote hist into the provided location
        
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

//Struct for taking the message and breaking it into its parts
typedef struct Message{
	char * type;
	int length;
	char * content;
} Message;

//Function to convert read() message to a Message struct
Message * convertInput(char * input, int i, Connection * con){
	//Check to see if the very last index is a '|' character. The main code will already give the client another chance.
	if(input[strlen(input) - 1] != '|'){
		printf("%d	ERR|4|M%dFT|\n", i, i);
		char error[] = "ERR|4|MxFT|";
		error[7] = i + '0';
		
		//Write the error message to the client and return NULL
		write(con->fd, error, strlen(error));
		return NULL;
	}
	
	
	
	//Now we can get started with the tokenizing
	Message * x = malloc(sizeof(Message));
	char * inputCopy = malloc(strlen(input) + 1);
	strcpy(inputCopy, input);
	char * split = strtok(inputCopy, "|");
	
	//Make sure that strtok split didn't return NULL; if it did, we don't have the right formatting
	if(split == NULL){
		printf("%d	ERR|4|M%dFT|\n", i, i);
		char error[] = "ERR|4|MxFT|";
		error[7] = i + '0';
		
		//Write the error message to the client and return NULL
		write(con->fd, error, strlen(error));
		free(x);
		free(inputCopy);
		return NULL;
	}
	
	//First, we want to check if the message is an ERR and return NULL if so
	if(strcmp(split, "ERR") == 0){
		printf("%d			%s\n", i, input);
		free(x);
		free(inputCopy);
		return NULL;
	} //Next we check to see if the type isn't "REG"
	else if(strcmp(split, "REG") != 0){
		printf("%d	ERR|4|M%dFT|\n", i, i);
		char error[] = "ERR|4|MxFT|";
		error[7] = i + '0';
		
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
		printf("%d	ERR|4|M%dFT|\n", i, i);
		char error[] = "ERR|4|MxFT|";
		error[7] = i + '0';
		
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
		printf("%d	ERR|4|M%dLN|\n", i, i);
		char error[] = "ERR|4|MxLN|";
		error[7] = i + '0';
		
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
		printf("%d	ERR|4|M%dFT|\n", i, i);
		char error[] = "ERR|4|MxFT|";
		error[7] = i + '0';
		
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
		printf("%d	ERR|4|M%dLN|\n", i, i);
		char error[] = "ERR|4|MxLN|";
		error[7] = i + '0';
		
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

void * joke(void * arg){
    char host[100], port[10], buf[101];
    Connection * c = (Connection *) arg;
    int error, nread;
	
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
	
    printf("\n[%s:%s] connection\n", host, port);
	
	
	
	//Send "knock knock" to client
	char knock[] = "REG|13|Knock, knock.|";
	write(c->fd, knock, strlen(knock));
	printf("%s:0		Knock, knock.\n", port);
	
	
	
	//Wait for "who's there" from client
	while(0 < (nread = read(c->fd, buf, 100))){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
        //printf("[%s:%s] read %d bytes %s\n", host, port, nread, buf);
    }
	if(nread < 7){
		return NULL;
	}
	//Split the input into tokens and check if it's valid
	Message * whosThere = convertInput(buf, 1, c);
	if(whosThere == NULL){
		close(c->fd);
		free(c);
		return NULL;
	}
	else if(strcmp(whosThere->content, "Who's there?") != 0){
		printf("1		ERR|4|M1CT|\n");
		char error[] = "ERR|4|M1CT|";
		write(c->fd, error, strlen(error));
		//Close the socket and return NULL
		close(c->fd);
		free(c);
		return NULL;
	}
	printf("%s:1			%s\n", port, whosThere->content);
	
	
	
	//Send "who" response
	char response[] = "REG|3|Ya.|";
	write(c->fd, response, strlen(response));
	printf("%s:2		Ya.\n", port);
	
	
	
	//Wait for "Ya, who?" from client
	if((nread = read(c->fd, buf, 100)) > 0){
        buf[nread] = '\0';
        //printf("[%s:%s] read %d bytes %s\n", host, port, nread, buf);
    }
	if(nread < 7){
		return NULL;
	}
	//Split the input into tokens and check if it's valid
	Message * whoWho = convertInput(buf, 3, c);
	if(whoWho == NULL){
		close(c->fd);
		free(c);
		return NULL;
	}
	//Check to see if the content actually matches the expected value
	else if(strcmp(whoWho->content, "Ya, who?") != 0){
		printf("3		ERR|4|M3CT|\n");
		char error[] = "ERR|4|M3CT|";
		write(c->fd, error, strlen(error));
		//Close the socket and return NULL
		close(c->fd);
		free(c);
		return NULL;
	}
	printf("%s:3				%s\n", port, whoWho->content);
	
	
	
	//Send the punchline to the client
	char punch[] = "REG|46|No thanks, I don't like having my data stolen.|";
	write(c->fd, punch, strlen(punch));
	printf("%s:4		No thanks, I don't like having my data stolen.\n", port);
	
	
	
	//Wait for disgust from client
	if((nread = read(c->fd, buf, 100)) > 0){
        buf[nread] = '\0';
		//printf("[%s:%s] read %d bytes %s\n", host, port, nread, buf);
    }
	if(nread < 7){
		return NULL;
	}
	
	Message * disgust = convertInput(buf, 5, c);
	if(disgust == NULL){
		close(c->fd);
		free(c);
		return NULL;
	}
	printf("%s:5				%s\n", port, disgust->content);
	
	
	
    close(c->fd);
    free(c);
    return NULL;
}
