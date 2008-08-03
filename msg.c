/*
 * Listen, and send back a static message to anyone that connects.
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


const char* foo =
	"Sorry, the hub is down. It will be back up shortly.|"
	;

int main(int argc, char** argv)
{
    if(argc < 2) {
        printf("Usage: %s listen_port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in listen_sock;
    memset(&listen_sock, 0, sizeof(struct sockaddr_in));
    listen_sock.sin_family = AF_INET;
    listen_sock.sin_addr.s_addr = INADDR_ANY;
    listen_sock.sin_port = htons(atoi(argv[1]));

    if(bind(listen_fd, (struct sockaddr*)&listen_sock, sizeof(struct sockaddr_in)) != 0) {
        printf("bind() failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	// listen
    if(listen(listen_fd, 5) != 0) {
        printf("listen() failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

	const int foolen = strlen(foo);
    while(1) {
        int c = accept(listen_fd, NULL, NULL);
        write(c, foo, foolen);
        close(c);
    }
}
