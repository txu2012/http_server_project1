#ifndef HTTPD_H
#define HTTPD_H

#include <string>
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>

using namespace std;

void start_httpd(unsigned short port, string doc_root);

// Handle error with user msg
void DieWithUserMessage(const char *msg, const char *detail);
// Handle error with sys msg
void DieWithSystemMessage(const char *msg);
// Print socket address
void PrintSocketAddress(const struct sockaddr *address, FILE *stream);
// Test socket address equality
bool SockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2);
// Create, bind, and listen a new TCP server socket
void HTTPClient(int clntSocket);
// Create and connect a new TCP client socket


enum sizeConstants {
  MAXSTRINGLENGTH = 128,
  BUFSIZE = 8192
};

#endif // HTTPD_H
