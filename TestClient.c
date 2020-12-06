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
		printf("ERR|4|M%dFT|\n", i);
		char error[] = "ERR|4|MxFT|";
		error[7] = i + '0';
		
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
		printf("ERR|4|M%dLN| A\n", i);
		char error[] = "ERR|4|MxLN|";
		error[7] = i + '0';
		
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
		x->content[strlen(split) - 1] = ',';
		//x->length--;
	}
	else{
		//Else it's a format error
		printf("ERR|4|M%dFT|\n", i);
		char error[] = "ERR|4|MxFT|";
		error[7] = i + '0';
		
		//Write the error message to the client and return NULL
		write(sock, error, strlen(error));
		free(x->content);
		free(x->type);
		free(x);
		return NULL;
	}
	//And then check if the message length is the same as reported
	if(x->length != strlen(x->content)){
		printf("ERR|4|M%dLN| B\n", i);
		char error[] = "ERR|4|MxLN|";
		error[7] = i + '0';
		
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

void clientCorrect(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;

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
	int nread;
	char buf[101];
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	
	
	//Send "who's there" response to the server
	char whoThere[] = "REG|12|Who's there?|";
	write(sock, whoThere, strlen(whoThere));
	
	
	
	//Receive the response
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	//Create the "response who?" message
	Message * response = convertInput(buf, 2, sock);
	
	char whoLength[10];
	sprintf(whoLength, "%ld", strlen(response->content) + 5);
	char * whoWho = malloc(strlen(response->content) + strlen(whoLength) + 12);
	
	strcpy(whoWho, "REG|");
	strcat(whoWho, whoLength);
	strcat(whoWho, "|");
	strcat(whoWho, response->content);
	strcat(whoWho, " who?|");
	
	
	printf("%s\n", whoWho);
	//Send the "response who?" message
	write(sock, whoWho, strlen(whoWho));
	
	
	
	//Receive the punchline
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	
	
	//React in disgust
	char react[] = "REG|4|Ugh.|";
	write(sock, react, strlen(react));
	
	
	
	// close the socket
	close(sock);

	return;	
}

void wrong1(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;

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
	int nread;
	char buf[101];
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
    
    //Send incorrect "who's there" response to the server
	char whoThere[] = "REG|12|Who is it?|";
	write(sock, whoThere, strlen(whoThere));
	
	//Listen for error from server
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	return;
}

void wrong2(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;

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
	int nread;
	char buf[101];
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
    
    //Send incorrect "who's there" response to the server
	char whoThere[] = "REG|11|Who's there?|";
	write(sock, whoThere, strlen(whoThere));
	
	//Listen for error from server
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	return;
}

void wrong3(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;

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
	int nread;
	char buf[101];
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
    
    //Send incorrect "who's there" response to the server
	char whoThere[] = "REG12Who is it?";
	write(sock, whoThere, strlen(whoThere));
	
	//Listen for error from server
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	return;
}

void wrong4(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;

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
	int nread;
	char buf[101];
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
    
    //Send incorrect "who's there" response to the server
	char whoThere[] = "REG|12Who is it?";
	write(sock, whoThere, strlen(whoThere));
	
	//Listen for error from server
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	return;
}

void err0(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;

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
	int nread;
	char buf[101];
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
    
    //Send incorrect "who's there" response to the server
	char whoThere[] = "ERR|4|M0FT|";
	write(sock, whoThere, strlen(whoThere));
	
	//Listen for error from server
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	return;
}





void pieces(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;

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
	int nread;
	char buf[101];
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
		printf("read %d bytes %s\n", nread, buf);
    }
    
    
    
    //Send "Who's there?" to the server in pieces
	char whoThere[] = "REG|12|Who's there?|";
	write(sock, "REG|", 4);
	write(sock, "12|", 3);
	write(sock, "Who's there?|", 13);
	
	
	
	//Receive the response
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	//Create the "response who?" message
	Message * response = convertInput(buf, 2, sock);
	
	char whoLength[10];
	sprintf(whoLength, "%ld", strlen(response->content) + 5);
	char * whoWho = malloc(strlen(response->content) + strlen(whoLength) + 12);
	
	strcpy(whoWho, "REG|");
	strcat(whoWho, whoLength);
	strcat(whoWho, "|");
	strcat(whoWho, response->content);
	strcat(whoWho, " who?|");
	
	
	printf("%s\n", whoWho);
	//Send the "response who?" message
	write(sock, whoWho, strlen(whoWho));
	
	
	
	//Receive the punchline
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	
	
	//React in disgust
	char react[] = "REG|4|Ugh.|";
	write(sock, react, strlen(react));
	
	
	
	// close the socket
	close(sock);

	return;	
}





void timeout(char * argv[]){
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	int i;

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
	int nread;
	char buf[101];
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
		printf("read %d bytes %s\n", nread, buf);
    }
    
    
    
    //Send "Who's there?" to the server in pieces
	char whoThere[] = "REG|12|Who's there?";
	write(sock, whoThere, strlen(whoThere));
	
	
	
	//Receive the response
	if((nread = read(sock, buf, 100)) > 0){
        buf[nread] = '\0';
        printf("read %d bytes %s\n", nread, buf);
    }
	
	
	
	// close the socket
	close(sock);

	return;	
}







int main(int argc, char * argv[]){
	
	if (argc < 3) {
		printf("Usage: %s [host] [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	//clientCorrect(argv);
	/*wrong1(argv);
	wrong2(argv);
	wrong3(argv);
	wrong4(argv);
	err0(argv);
	pieces(argv);*/
	timeout(argv);
	
	return EXIT_SUCCESS;
}
