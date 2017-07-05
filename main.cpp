#include <iostream>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "httpd.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
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

void usage(char * argv0)
{
	cerr << "Usage: " << argv0 << " listen_port docroot_dir pool/nopool num_pool" << endl;
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		usage(argv[0]);
		return 1;
	}

	long int port = strtol(argv[1], NULL, 10);

	if (errno == EINVAL || errno == ERANGE) {
		usage(argv[0]);
		return 2;
	}

	if (port <= 0 || port > USHRT_MAX) {
		cerr << "Invalid port: " << port << endl;
		return 3;
	}

	string doc_root = argv[2];

	start_httpd(port, doc_root);
  
	return 0;
}
