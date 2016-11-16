
const char * usage =
"                                                               \n"
"daytime-server:                                                \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   daytime-server <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             \n"
"                                                               \n"
"In another window type:                                       \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server  \n"
"is running. <port> is the port number you used when you run   \n"
"daytime-server.                                               \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and   \n"
"the time of the day.                                          \n"
"                                                               \n";


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

int QueueLength = 5;

// Processes time request
void processTimeRequest( int socket );
void processRequest ( int socket );
void processRequestThread(int);
void poolSlave(int);
int endsWith(char* start, const char* end);



extern "C" void killzombie(int sig)
{//wait until the child process terminates
   // int pid = wait3(0, 0, NULL);
    while(waitpid(-1, NULL, WNOHANG) > 0); //return immediately if no child has exited
}


int
main( int argc, char ** argv )
{
  
  char mode;
  int port = 0;
  
  if(argc > 1) {
	if(!strcmp(argv[1], "-f")) {
		mode = 'f';
		port = 3212;
	}
	else if(!strcmp(argv[1], "-t")) {
		mode = 't';
		port = 3212;
	}
	else if(!strcmp(argv[1], "-p")) {
		mode = 'p';
		port = 3212;
	}
	else if(atoi(argv[1]) > 65536 || atoi(argv[1]) < 1024){
	//	help();
		return 1;
	}
	else {
		port = atoi(argv[1]);
	}
  }
  
  if(argc > 2) {
	if(atoi(argv[2]) > 65536 || atoi(argv[2]) < 1024){
		return 1;
         }       
         else {  
	 	port = atoi(argv[2]);
	 }
  }
  // Print usage if not enough arguments
  if ( argc < 2 ) {
    fprintf( stderr, "%s", usage );
    exit( -1 );
  }

  // Get the port from the arguments
  // port = atoi( argv[1] );


	struct sigaction signalAction;
	signalAction.sa_handler = killzombie;	
	sigemptyset(&signalAction.sa_mask);
	signalAction.sa_flags = SA_RESTART;
	int errok = sigaction(SIGCHLD, &signalAction, NULL );

	if ( errok ) 
	{
		perror( "sigaction" );
		exit( -1 );
	}
 
 
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }


	if(mode == 'p') {
  		pthread_t tid[5]; 
		pthread_attr_t attr;
		for(int i = 0; i< 5; i++) {
			pthread_create(&tid[i], &attr, (void * (*)(void*))poolSlave, (void *)masterSocket);    		
		}
		pthread_join(tid[0], NULL); 
    	}
  else {
	  while ( 1 ) {

		// Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket,
					  (struct sockaddr *)&clientIPAddress,
					  (socklen_t*)&alen);

		
		if ( slaveSocket < 0 ) {
		  perror( "accept" );
		  exit( -1 );
		}

		if(mode == 't') {
			pthread_t tid;
			pthread_attr_t attr;

			pthread_attr_init(&attr);
			pthread_create(&tid, &attr, (void * (*)(void *))processRequest, (void *)slaveSocket);
		}
		else if(mode == 'f') {
			int pid = fork();
			if(pid == 0) {
				processRequest( slaveSocket );
				shutdown(slaveSocket, 2);
				// Close socket
				close( slaveSocket );
				exit(707);
			}
			close( slaveSocket );
		}
		else {
			processRequest(slaveSocket);
			shutdown(slaveSocket, 2);
			close(slaveSocket);
		}	
	  }
  }
}

void processRequestThread(int socket) {
	processRequest(socket);
	shutdown(socket, 2);
	close(socket);
}

void poolSlave(int masterSocket)
{
	while(1) {
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket,
					  (struct sockaddr *)&clientIPAddress,
					  (socklen_t*)&alen);

		
		if ( slaveSocket < 0 ) {
		  perror( "accept" );
		  exit( -1 );
		}
		processRequest(slaveSocket);
		shutdown(slaveSocket, 2);
		close(slaveSocket);
	}
}

void
processRequest( int fd )
{
 const int MaxName = 1024;
 char name[ MaxName + 1 ];
 int length = 0;
 int start = 0;
 int n, gotGet = 0;
 unsigned char oldChar = 0;
 unsigned char newChar = 0;
 char docPath[MaxName + 1];
 int got_doc = 0;
 char curr_string[MaxName + 1];
 char* contentType = (char *)malloc(sizeof(char) * 50);

	while(( n = read( fd, &newChar, sizeof(newChar))) > 0 ) {

		length++;
		if(newChar == ' ') {
			if(gotGet == 0){
				gotGet = 1;
				start = length;
			}
			else if (got_doc == 0){
				curr_string[length - 1] = 0;
				got_doc = 1;
			}
				strcpy(docPath, curr_string + start);
		}
		else if(newChar == '\n' && oldChar == '\r') {
			break;
		}
		else {
			oldChar = newChar;
				curr_string[length - 1] = newChar;
		}
	}
	//getting details of request
	
	char cwd[256] = {0};
	getcwd(cwd, 256);

	if(strcmp(docPath, "/") == 0) {
		strcat(cwd, "/http-root-dir/htdocs/index.html");
	}

	else if(strstr(docPath, "/icons")) {
		strcat(cwd, "/http-root-dir");
		strcat(cwd, docPath);
	}
	else if(strstr(docPath, "/htdocs")) {
		 strcat(cwd, "/http-root-dir");
		 strcat(cwd, docPath);
	}
	else {//else make filepath
		strcat(cwd, "/http-root-dir/htdocs");
		strcat(cwd, docPath);
	}


	if(!endsWith(cwd, ".html") || !endsWith(cwd, ".html/")){
		strcpy(contentType, "text/html");
	}
	else if(!endsWith(cwd, ".gif") || !endsWith(cwd, ".gif/")){
		strcpy(contentType, "image/gif");
	}
	else {
		strcpy(contentType, "text/plain");
	}

	int docs = open(cwd, O_RDONLY);

	const char* clrf = "\r\n";
	const char *protocol = "HTTP/1.0 404 File Not Found " ;
	const char *server = "Server: Folasade ";
	const char *content = "Content-type: ";
	const char *errorMessage = "FILE NOT FOUND!!";

	if(docs < 0) {
		write(fd, protocol, strlen(protocol));
		write(fd, "\r\n", 2);
		write(fd, server, strlen(server));
		write(fd, "\r\n", 2);
		write(fd, content, strlen(content));
	    	write(fd, contentType, strlen(contentType));
		write(fd, clrf, strlen(clrf));
		write(fd, clrf, strlen(clrf));
		write(fd, errorMessage, strlen(errorMessage));
	}
	else {
		write(fd, "HTTP/1.1 200 OK", 15);
        	write(fd, "\r\n", 2);
        	write(fd, server, strlen(server));
		write(fd, "\r\n", 2);
		write(fd, content, strlen(content));
		write(fd, contentType, strlen(contentType));
	    	write(fd, clrf, strlen(clrf));
		write(fd, clrf, strlen(clrf));

		int count = 0;
		char c;
		while(count = read(docs, &c, 1)) {
			if(write(fd, &c, 1) != count) {
			 	//perror(“write”);
				count = 0;
			}	
		} 
	}
	//Expanding filePath
	//if(strstr(docPath, "..") != 0){
	//}	

	/*File *docs;
		if(textcheck == true) {
			s = fopen(cwd, "r");		
		}
	*/
	//printf("%s \n", docPath);
	//printf("%s \n", cwd);
}

int endsWith(char* start, const char* end)
{
	int length = strlen(start);
	int ending = strlen(end);
	int difference = length - ending;

	return strcmp(start + difference, end);
}

void
processTimeRequest( int fd )
{
  // Buffer used to store the name received from the client
  const int MaxName = 1024;
  char name[ MaxName + 1 ];
  int nameLength = 0;
  int n;

  // Send prompt
  const char * prompt = "\nType your name:";
  write( fd, prompt, strlen( prompt ) );

  // Currently character read
  unsigned char newChar;

  // Last character read
  unsigned char lastChar = 0;

  //
  // The client should send <name><cr><lf>
  // Read the name of the client character by character until a
  // <CR><LF> is found.
  //
    
  while ( nameLength < MaxName &&
	  ( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {

    if ( lastChar == '\015' && newChar == '\012' ) {
      // Discard previous <CR> from name
      nameLength--;
      break;
    }

    name[ nameLength ] = newChar;
    nameLength++;

    lastChar = newChar;
  }

  // Add null character at the end of the string
  name[ nameLength ] = 0;

  printf( "name=%s\n", name );

  // Get time of day
  time_t now;
  time(&now);
  char	*timeString = ctime(&now);

  // Send name and greetings
  const char * hi = "\nHi ";
  const char * timeIs = " the time is:\n";
  write( fd, hi, strlen( hi ) );
  write( fd, name, strlen( name ) );
  write( fd, timeIs, strlen( timeIs ) );
  
  // Send the time of day 
  write(fd, timeString, strlen(timeString));  // Send last newline
  const char * newline="\n";
  write(fd, newline, strlen(newline));
  
}

