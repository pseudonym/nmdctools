/*
 * Direct Connect chat-only forwarder. Not designed to be pretty or fake shares
 * on behalf of its users, and it silently drops searches and download requests
 * instead of giving some kind of error message. Requires libevent (anything
 * above 1.1 should be fine); use -levent when compiling.
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

#include <event.h>

struct connectionpair {
	int client;
	struct bufferevent* clientbuf;
	int hub;
	struct bufferevent* hubbuf;
};

void readcb(struct bufferevent*, void* arg);
void writecb(struct bufferevent*, void* arg);
void errorcb(struct bufferevent*, short what, void* arg);
void read_listen_cb(int fd, short event, void* arg);

const char*const ctm = "$ConnectToMe ";
const char*const rcm = "$RevConnectToMe ";
const char*const search = "$Search ";
int ctmlen;
int rcmlen;
int searchlen;
struct sockaddr_in destaddr;

int main(int argc, char** argv)
{
	// might as well initialize these here....
	ctmlen = strlen(ctm);
	rcmlen = strlen(rcm);
	searchlen = strlen(search);

	if(argc < 4) {
		printf("Usage: %s listen_port foreign_ip_address foreign_port\n", argv[0]);
		printf("Sorry, haven't gotten around to DNS resolution, you have to do it yourself\n");
		exit(EXIT_FAILURE);
	}

	// set up destination address sockaddr
	memset(&destaddr, 0, sizeof(struct sockaddr_in));
	destaddr.sin_family = AF_INET;
	destaddr.sin_addr.s_addr = inet_addr(argv[2]);
	destaddr.sin_port = htons(atoi(argv[3]));

	event_init(); // start libevent library

	// set up listen socket
	int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
	struct sockaddr_in listen_sock;
	memset(&listen_sock, 0, sizeof(struct sockaddr_in));
	listen_sock.sin_family = AF_INET;
	listen_sock.sin_addr.s_addr = INADDR_ANY;
	listen_sock.sin_port = htons(atoi(argv[1]));

	// bind listen socket
	if(bind(listen_fd, (struct sockaddr*)&listen_sock, sizeof(struct sockaddr_in)) != 0) {
		printf("bind() failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// set non-blocking
	int f;
	if((f = fcntl(listen_fd, F_GETFL, 0)) < 0) {
		printf("fcntl() get failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	f |= O_NONBLOCK;
	if(fcntl(listen_fd, F_SETFL, f) < 0) {
		printf("fcntl() set failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// listen
	if(listen(listen_fd, 5) != 0) {
		printf("listen() failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// create event, wait for connections
	struct event listen_ev;
	event_set(&listen_ev, listen_fd, EV_READ | EV_PERSIST, read_listen_cb, NULL);
	event_add(&listen_ev, NULL);

	return event_dispatch(); // run main event loop
}

void readcb(struct bufferevent* bufev, void* arg)
{
	struct connectionpair* fds = (struct connectionpair*)arg;
	struct evbuffer* buf = EVBUFFER_INPUT(bufev);
	struct bufferevent* other = (bufev == fds->clientbuf ? fds->hubbuf : fds->clientbuf);

	u_char* p;
	// keep going while we have lines left in the buffer
	while((p = evbuffer_find(buf, (u_char*)"|", 1))) {
		u_char* dat = EVBUFFER_DATA(buf);
		int len = p - dat + 1;
		// if it's a command we don't allow, drain the buffer and continue
		if(len > ctmlen && !memcmp(ctm, dat, ctmlen)) {
			evbuffer_drain(buf, len);
			continue;
		}
		if(len > rcmlen && !memcmp(rcm, dat, rcmlen)) {
			evbuffer_drain(buf, len);
			continue;
		}
		if(len > searchlen && !memcmp(search, dat, searchlen)) {
			evbuffer_drain(buf, len);
			continue;
		}
#ifndef NO_DISPLAY_CHAT
		// write chat messages to stdout
		if(len && *dat != '$') {
			fwrite(dat, 1, len, stdout);
			putchar('\n');
		}
#endif
		// write allowed message to other end of connection
		bufferevent_write(other, dat, len);
		evbuffer_drain(buf, len);
	}
	// make sure the other end will write
	bufferevent_enable(other, EV_WRITE);
}

void writecb(struct bufferevent* bufev, void* arg)
{
	// only called when we have no data left to send,
	// so disable writes until we get something
	bufferevent_disable(bufev, EV_WRITE);
}

void errorcb(struct bufferevent* bufev, short what, void* arg)
{
	// on disconnect, free everything
	struct connectionpair* fds = (struct connectionpair*)arg;
	puts("connection terminated\n");
	close(fds->client);
	close(fds->hub);
	bufferevent_free(fds->clientbuf);
	bufferevent_free(fds->hubbuf);
	free(fds);
}

void read_listen_cb(int fd, short event, void* arg)
{
	// when we get an incoming connection, connect to hub and
	// allocate structures so we can pass data between them
	struct connectionpair* fds = malloc(sizeof(struct connectionpair));
	fds->client = accept(fd, NULL, NULL);
	fds->hub = socket(PF_INET, SOCK_STREAM, 0);

	if(fds->client < 0 || fds->hub < 0)
		printf("failed to make sockets\n");

	// set non-block on client
	int f;
	if((f = fcntl(fds->client, F_GETFL, 0)) < 0) {
		printf("fcntl() get failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	f |= O_NONBLOCK;
	if(fcntl(fds->client, F_SETFL, f) < 0) {
		printf("fcntl() set failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// set non-block on hub connection
	if((f = fcntl(fds->hub, F_GETFL, 0)) < 0) {
		printf("fcntl() get failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	f |= O_NONBLOCK;
	if(fcntl(fds->hub, F_SETFL, f) < 0) {
		printf("fcntl() set failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// connect to hub
	int ret = connect(fds->hub, (struct sockaddr*)&destaddr, sizeof(struct sockaddr_in));
	if(ret && errno != EINPROGRESS) {
		printf("connect() failed: %s\n", strerror(errno));
		close(fds->client);
		close(fds->hub);
		free(fds);
		return;
	}

	// create buffers and make them readable
	fds->clientbuf = bufferevent_new(fds->client, readcb, writecb, errorcb, fds);
	fds->hubbuf = bufferevent_new(fds->hub, readcb, writecb, errorcb, fds);

	bufferevent_enable(fds->hubbuf, EV_READ);
	bufferevent_enable(fds->clientbuf, EV_READ);
}

