/*
 * Listen, and send back the mesage read from stdin if anyone connects.
 *
 * Written by Matt Pearson <mjp64@cornell.edu>.
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFFER_SIZE 1024 // should be large enough...

int main(int argc, char** argv)
{
	char msg[BUFFER_SIZE];
	int len;

	if(argc < 2) {
		fprintf(stderr, "Usage: %s listen_port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	len = fread(msg, 1, BUFFER_SIZE, stdin);
	if(!feof(stdin)) {
		fprintf(stderr, "Message too long (stop the sesquipedalian loquaciousness)\n");
		exit(EXIT_FAILURE);
	}

	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in listen_sock;
	memset(&listen_sock, 0, sizeof(struct sockaddr_in));
	listen_sock.sin_family = AF_INET;
	listen_sock.sin_addr.s_addr = INADDR_ANY;
	listen_sock.sin_port = htons(atoi(argv[1]));

	if(bind(listen_fd, (struct sockaddr*)&listen_sock, sizeof(struct sockaddr_in)) != 0) {
		perror("bind() failed");
		exit(EXIT_FAILURE);
	}

	// listen
	if(listen(listen_fd, 5) != 0) {
		perror("listen() failed");
		exit(EXIT_FAILURE);
	}

	while(1) {
		int c = accept(listen_fd, NULL, NULL);
		write(c, msg, len);
		close(c);
	}
}
