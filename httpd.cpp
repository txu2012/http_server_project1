#include <iostream>
#include "httpd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <time.h>

using namespace std;
static const int MAXPENDING = 5; // Maximum outstanding connection requests
char* docRoot;

// Thread objects to use multithreading 
struct ThreadInput{
  int clientSock;
};

// Request object to store and send back to client
struct RequestMessage{
  char* name;
  char* port;
  char* path;
  char* type;
  char* getMethod;
  char* header;
  char* userAgent;
  char* version;
  char* connection;
  char* file;
};

// Handles Threads (Used from example)
void *ThreadServer(void *threads){
  pthread_detach(pthread_self());
  // Extract socket file descriptor from argument
  int clntSock = ((struct ThreadInput *) threads)->clientSock;
  free(threads); // Deallocate memory for argument
  HTTPClient(clntSock);
  
  return (NULL);
}

/*
 * Grabs the header and parses them for sending back to client
 *
 */
void HTTPHeader(struct RequestMessage* head, bool* connectionClose, bool* checkHeader){
  // Grab initial line for later
  char* header = strtok(head->header,"\r\n");
  
  // Grab first header line (Host name typically)
  char* keys = strtok(NULL,"\r\n"); 

  int index = 0;
  
  // Loop runs until there are no more lines
  while(keys != NULL){
    const char host_id[] = "host:";
    const char user_agent_id[] = "user-agent:";
    const char connection_id[] = "connection:";
    char hostName[BUFSIZE];
    char portNumber[BUFSIZE];
    char user_agent[BUFSIZE];
    char connectionType[BUFSIZE];
    bool foundHost = false;
    bool found_user_agent = false;
    bool found_connection = false;
    unsigned int hostCounter = 0;
    unsigned int userCounter = 0;
    unsigned int connCounter = 0;
    
    // Checks header if it is a host value
    for(unsigned int i = 0; i < strlen(host_id);i++){
      if(tolower(keys[i]) == tolower(host_id[i])){
        hostCounter++;
      }
      if(hostCounter == strlen(host_id)){
        foundHost = true;
      }
    }
    
    // Checks header if it is a user_agent value
    for(unsigned int i = 0; i < strlen(user_agent_id);i++){
      if(tolower(keys[i]) == tolower(user_agent_id[i])){
        userCounter++;
      }
      if(userCounter == strlen(user_agent_id)){
        found_user_agent = true;
      }
    }
    // Checks header if it is a connection value
    for(unsigned int i = 0; i < strlen(connection_id);i++){
      if(tolower(keys[i]) == tolower(connection_id[i])){
        connCounter++;
      }
      if(userCounter == strlen(user_agent_id)){
        found_connection = true;
      }
    }
    
    // If Host is found on header, insert into request information
    if(foundHost == true){
      for(unsigned int i = strlen(host_id)+1; i < strlen(keys);i++){
        hostName[index] = keys[i];
        if(keys[i] == ':'){
          index = 0;
          for(unsigned int j = i+1; j < strlen(keys);j++){
            portNumber[index] = keys[j];
            index++;
          }
          break;
        }
        index++;
      }
      index = 0;
      // Allocates memory for the pointers
      head->name = (char *) malloc(strlen(hostName));
      head->port = (char *) malloc(strlen(portNumber));
      // Copies string into object
      strcpy(head->name, hostName);
      
      // If port was found after host, insert. Else set default port number
      if(portNumber != NULL){
        head->port = (char *) malloc(strlen(portNumber));
        strcpy(head->port, portNumber);
      }
      else{
        head->port = (char *) malloc(4);
        strcpy(head->port, "1000");
      }
    }
    
    // If User Agent is found on header, insert into request information
    if(found_user_agent == true){
      index = 0;
      // Searches for colon and copies after the colon
      for(unsigned int i = strlen(user_agent_id)+1; i < strlen(keys); i++){
         if(keys[i] == ':'){
           for(unsigned int j = i+1; j < strlen(keys);j++){
             user_agent[index] = keys[j];
             index++;
           }
           break;
        }
        index++;
      }
      head->userAgent = (char* )malloc((size_t) strlen(user_agent));
      strcpy(head->userAgent, user_agent);
    }
    // If a connection header is found, parse and store into request object
    if(found_connection == true){
      // Searches for colon and copies after the colon
      for(unsigned int i = strlen(user_agent_id)+1; i < strlen(keys); i++){
        if(keys[i] == ':'){
          for(unsigned int j = i+1; j < strlen(keys);j++){
            connectionType[index] = keys[j];
            index++;
          }
          break;
        }
        index++;
      }
      const char close[] = "close";
      unsigned int closeCounter = 0;
      // Checks if the connection value is close
      for(unsigned int i = 0; i < strlen(connectionType); i++){
        if(connectionType[i] == close[i]){
          closeCounter++;
        }
        if(closeCounter == strlen(close)){
          *connectionClose = true;
        }
        else{
          *connectionClose = false;
        }
      }
      head->connection = (char*) malloc((size_t)strlen(connectionType));
      memcpy(head->connection, connectionType, strlen(connectionType));
      
    }
    index = 0;
    
    // Grabs next line
    keys = strtok(NULL,"\r\n");
  }
  
  // Checks if the header has a valid name, user agent, and port
  if(head->name == NULL || head->port == NULL || head->userAgent == NULL){
    *checkHeader = false;
    return;
  }
  // Grabs the method in the header if available
  char *methodType = strtok(header, " ");
  if(methodType == NULL){
    *checkHeader = false;
    return;
  }
  else{
    head->getMethod = (char*) malloc((size_t) strlen(methodType));
    strcpy(head->getMethod, methodType);
  }
  
  // Grabs the path and checks if there is a path in header
  char *dirPath = strtok(NULL, " ");
  if(dirPath == NULL){
    *checkHeader = false;
    return;
  }
  else{
    // Sets the path into the object, If there is no set path, put index.html as default
    if(strlen(dirPath) == 1 && *dirPath == '/'){
      head->path = (char*) malloc(12);
      head->type = (char*) malloc(10);
      head->file = (char*) malloc(12);
      strcpy(head->file, "index.html");
      strcpy(head->path, "/index.html");
      strcpy(head->type, "text/html");
    }
    else{
      head->path = (char*) malloc((size_t) strlen(dirPath));
      strcpy(head->path,dirPath);
      // Sets file to check later if file is available
      head->file = (char*) malloc(12);
      head->file = dirPath + 1;
      // Checks if the path is an html or a jpg type
      if(strstr(dirPath,".html") != NULL){
        head->type = (char*) malloc(10);
        strcpy(head->type, "text/html");
      }
      else if(strstr(dirPath,".jpg") != NULL || strstr(dirPath,".jpeg") != NULL){
        head->type = (char*) malloc(12);
        strcpy(head->type, "image/jpeg");
      }    
    }
  }
  // Grabs and checks if there is a version number in the header
  char *versionNumber = strtok(NULL, " ");
  if(versionNumber == NULL){
    *checkHeader = false;
    return;
  }
  else{
    head->version = (char*) malloc((size_t) strlen(versionNumber));
    strcpy(head->version, versionNumber); 
  }
}


/*
 * Parses the Message and searches for \r\n\r\n to separate for the header
 *
 */
void HTTPMessage(struct RequestMessage* recvMessage, char buffer[]){
    bool checkCRLF = false;
    // Checks for CRLF
    if(strstr(buffer,"\r\n\r\n")){
      checkCRLF = true;
    }
    
    // If CRLF is present, copy string until \r\n\r\n
    if(checkCRLF == true) {
      size_t i = strstr(buffer, "\r\n\r\n") - buffer;
      // If header is empty, copy until crlf
      if(recvMessage->header == NULL){
        recvMessage->header = (char*) malloc(i);
        strncpy(recvMessage->header,buffer,i);
      }
      // Else, combine the header with the new information
      else{
        // Allocate space for new size of char array with buffer and header
        char* temp = (char*) malloc(i + strlen(recvMessage->header));
        strcpy(temp,recvMessage->header);
        strcat(temp,buffer);
        // Allocate space in the request object for new size of char array with buffer and header
        recvMessage->header = (char*) malloc(strlen(temp));
        strcpy(recvMessage->header, temp);
      }
      return;
    }
    // If CRLF is not present, copy buffer into the request object for parsing
    else {
      if(recvMessage->header == NULL){
        recvMessage->header = (char*)malloc(strlen(buffer));
        strcpy(recvMessage->header,buffer);
      }
      // Else, combine the header with the new information
      else{
        // Allocate space for new size of char array with buffer and header
        char* temp = (char* )malloc(strlen(buffer) + strlen(recvMessage->header));
        strcpy(temp,recvMessage->header);
        strcat(temp,buffer);
        // Allocate space in the request object for new size of char array with buffer and header
        recvMessage->header = (char*) malloc(strlen(temp));
        strcpy(recvMessage->header, temp);
      }
      return;
    }
}

/*
 * HTTPRespose for sending the header back to the client
 *
 */
void HTTPResponse(int response, int clientSock, int length, char* type, char time_file[], bool* connect){
  char* sentCode = (char*) malloc(20);
  // Checks if the server response is ok or error
  // If response is not 200, send appropriate error message
  if(response != 200){
    // If 404 code was sent, send Not Found
    if(response == 404){
      strcpy(sentCode,"Not Found");
    }
    else if(response == 403){
      strcpy(sentCode,"Forbidden");
    }
    // Else if 400 code was sent, send Client Error
    else if(response == 400){
      strcpy(sentCode,"Client Error");
    }
    printf("HTTP/1.1 %d %s\r\n", response, sentCode);
    printf("Server: TritonHTTP\r\n");
    
    // Sets the header for error and gets length of sent header
    char* header = (char *)malloc(501);
    snprintf(header, 501, "HTTP/1.1 %d %s\r\nServer: TritonHTTP\r\n\r\n", response, sentCode);
    int len = strlen(header);
    
    // Send header to client
    send(clientSock, header, len, 0);
  }
  // If the response is 200, send full header
  else if(response == 200){
    strcpy(sentCode,"OK");
    
    // Prints to server for ease of knowing header information
    printf("HTTP/1.1 %d %s\r\n", response, sentCode);
    printf("Server: TritonHTTP\r\n");
    printf("Last-Modified: %s\r\n", time_file);
    printf("Content-Type: %s\r\n", type);
    printf("Content-Length: %d\r\n", length);
    // Checks if there is a connection close value
    if(*connect == true){
      printf("Connection: close\r\n");
      
      // Gets the length of header string with all content to send back number of bytes
      char* header = (char *)malloc(501);
      snprintf(header, 501, "HTTP/1.1 %d %s\r\nServer: TritonHTTP\r\nLast-Modified: %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", response, sentCode, time_file, type, length);
      int len = strlen(header);
      
      // Send header to client
      send(clientSock, header, len, 0);
    }
    else{
      // Gets the length of header string with all content to send back number of bytes
      char* header = (char *)malloc(501);
      snprintf(header, 501, "HTTP/1.1 %d %s\r\nServer: TritonHTTP\r\nLast-Modified: %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", response, sentCode, time_file, type, length);
      int len = strlen(header);
      
      // Send header to client
      send(clientSock, header, len, 0);
    }
  }
}

// Handles the TCP connections for each thread
void HTTPClient(int clientSocket) {
  char buffer[BUFSIZE]; // Buffer for echo string
  ssize_t numBytesRcvd;
  RequestMessage recvNewMessage;
  
  // Timer set for timeout if too long (5 seconds)
  struct timeval timer;
  timer.tv_sec = 5;
  timer.tv_usec = 5000;

  setsockopt(clientSocket,SOL_SOCKET,SO_RCVTIMEO,&timer,sizeof(timer));

  // Continues to loop until the header is complete, or alarm goes off and closes connection
  while(true){
    // Receive message from client
    numBytesRcvd = recv(clientSocket, buffer, BUFSIZE,0);
    if (numBytesRcvd <= 0){
      // If timer reaches 5, time the client out and close socket
      if(errno==EAGAIN || errno==EWOULDBLOCK){
        DieWithSystemMessage("recv() timed out");
        close(clientSocket);
        return;
      }
      else{
        DieWithSystemMessage("recv() failed");
        close(clientSocket);
        return;
      }
    }
    // Create new HTTPRequest object
    //RequestMessage recvNewMessage;
    bool connectionClose = false;
    bool checkHeader = true;
    
    // Grabs the header from the message
    HTTPMessage(&recvNewMessage,buffer);
    
    // Parses the header into respective sections
    HTTPHeader(&recvNewMessage, &connectionClose, &checkHeader);
    
    // Creates and combines the actual path of root and header path
    char* combinedPath = (char*)malloc(strlen(docRoot) + strlen(recvNewMessage.path));
    char newpath[BUFSIZE];
    strncpy(combinedPath,docRoot,strlen(docRoot));
    strcat(combinedPath,recvNewMessage.path);
    
    // Finds real path and sets it into combinedPath
    realpath(combinedPath, newpath);
    
    // Gets the status of the file
    struct stat file_obj;
    stat(recvNewMessage.file,&file_obj);
    
    // Creates the time set for the file last modified
    char time_file[1024];
    time_t t = file_obj.st_mtime;
    struct tm *gmt;
    gmt = gmtime(&t);
    strftime(time_file, 1024, "%a, %d %b %Y, %k:%M:%S %Z", gmt);
    
    // If the header was not complete, send client error
    if(checkHeader == false){
      HTTPResponse(400, clientSocket, file_obj.st_size, recvNewMessage.type,time_file,&connectionClose);
      break;
    }
    // Checks if the file is found by checking the status of file
    else if(stat(newpath,&file_obj) == -1){
      HTTPResponse(404, clientSocket, file_obj.st_size, recvNewMessage.type,time_file,&connectionClose);
      if(connectionClose == true){
        break;
      }
    }
    // Checks if the file is readable
    else if(!(file_obj.st_mode & S_IROTH)){
      HTTPResponse(403, clientSocket, file_obj.st_size, recvNewMessage.type,time_file,&connectionClose);
      if(connectionClose == true){
        break;
      }
    }
    // Else send header to client
    else{
      HTTPResponse(200, clientSocket, file_obj.st_size, recvNewMessage.type,time_file,&connectionClose);
      if(connectionClose == true){
        break;
      }
      // Send body after sending header
      FILE* doc = fopen(newpath, "r");
      off_t* size = 0;
      sendfile(clientSocket, fileno(doc), size, file_obj.st_size);
    }
  }
  close(clientSocket);
}

// Used from example codes
// Starts up the server and runs forever
void start_httpd(unsigned short port, string doc_root)
{
  cerr << "Starting server (port: " << port <<
		", doc_root: " << doc_root << ")" << endl;
  
  int serverSock;
  if ((serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    DieWithSystemMessage("socket() failed");
    
   // Construct local address structure
  struct sockaddr_in servAddr;                  // Local address
  memset(&servAddr, 0, sizeof(servAddr));       // Zero out structure
  servAddr.sin_family = AF_INET;                // IPv4 address family
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
  servAddr.sin_port = htons(port);          // Local port

  // Bind to the local address
  if (bind(serverSock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0)
    DieWithSystemMessage("bind() failed");

  // Mark the socket so it will listen for incoming connections
  if (listen(serverSock, MAXPENDING) < 0)
    DieWithSystemMessage("listen() failed");
 
  docRoot = (char*) malloc(doc_root.size());
  strcpy(docRoot,doc_root.c_str());
  
  for (;;) { // Run forever
    struct sockaddr_in clntAddr; // Client address
    // Set length of client address structure (in-out parameter)
    socklen_t clntAddrLen = sizeof(clntAddr);

    // Wait for a client to connect
    int clntSock = accept(serverSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
    if (clntSock < 0)
      DieWithSystemMessage("accept() failed");

    // clntSock is connected to a client!

    char clntName[INET_ADDRSTRLEN]; // String to contain client address
    if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr, clntName,
        sizeof(clntName)) != NULL){
      printf("Handling client %s/%d\n", clntName, ntohs(clntAddr.sin_port));
    }
    else{
      puts("Unable to get client address"); 
    }
    // Threads the clients into separate threads
    struct ThreadInput* clientInput = (struct ThreadInput *) malloc(sizeof(struct ThreadInput));
    pthread_t clientThread;
    if (clientInput == NULL){
      DieWithSystemMessage("malloc() failed");
    }
    // Sets thread for client socket
    clientInput->clientSock = clntSock;
    int returnValue = pthread_create(&clientThread, NULL, ThreadServer, clientInput);
    // Checks if thread was created
    if (returnValue != 0){
      DieWithUserMessage("pthread_create() failed", strerror(returnValue));
    }
    printf("with thread %ld\n", (long int) clientThread);
  }
}

