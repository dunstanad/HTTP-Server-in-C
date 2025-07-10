#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include "http.h"
#include "logger.h"

#define PORT "8080"
#define BACKLOG 10 // how many pending connections queue will hold
#define BUFFERSIZE 1024
#define MAX_EVENTS 20

// getaddrinfo() -> socket() -> setsockopt() -> bind() -> listen() -> accept() -> send() , recv() ->  close()

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{ // IPv4
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}
	// IPv6
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}



// helper to set a socket non-blocking
static int non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main()
{
	struct addrinfo hints, *res, *p;
	int rv;
	int listen_fd; // for binding, listening
	int opt = 1;
	int numbytes = 0;
	char buffer[BUFFERSIZE];
	char s[INET6_ADDRSTRLEN];
	char resp[] = "HTTP/1.0 200 OK\r\n"
                  "Server: webserver-c\r\n"
                  "Content-type: text/html\r\n\r\n"
                  "<html>hello, world</html>\r\n";


	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use local IP

	init_logger("server.log"); // start the logger
	set_log_level(LOG_DEBUG);

	if ((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	for (p = res; p != NULL; p = p->ai_next)
	{
		if ((listen_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("Server : socket");
			continue;
		}
		// To prevent Address already in use error
		if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		{
			perror("Server : setsockopt");
			exit(1);
		}

		if (bind(listen_fd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(listen_fd);
			perror("Server : bind");
			continue;
		}
		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(listen_fd, BACKLOG) == -1)
	{
		perror("listen");
		exit(1);
	}
	non_blocking(listen_fd); // make listening socket non-blocking

	// create epoll instance, watching the listening socket
	struct epoll_event ev, events[MAX_EVENTS];
	int epfd, nfds, i;

	epfd = epoll_create1(0);
	ev.events = EPOLLIN; // ready to read (i.e new connections)
	ev.data.fd = listen_fd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
	printf("Server started on port %s. Waiting for connections...\n", PORT);

	// event loop
	while (1)
	{
		nfds = epoll_wait(epfd, events, MAX_EVENTS, 15000); // wait 15 seconds
		if (nfds < 0)
		{
			if (errno == EINTR)
				continue;
			perror("epoll_wait");
			break;
		}
		for (i = 0; i < nfds; i++)
		{
			int fd = events[i].data.fd;

			// a) new incoming connection
			if (fd == listen_fd)
			{
				struct sockaddr_storage cli_addr; // store client address
				socklen_t cli_len = sizeof(cli_addr);
				int conn_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);

				if (conn_fd < 0)
					continue;
				non_blocking(conn_fd);
				ev.events = EPOLLIN | EPOLLET; // edge-triggered
				ev.data.fd = conn_fd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev);

				inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)&cli_addr), s, sizeof(s));
				printf("New connection from %s\n", s);
			}
			// Handle client data
			else{
				memset(buffer, 0, BUFFERSIZE);
				numbytes = recv(fd, buffer, BUFFERSIZE, 0);
				printf("Request:\n%s\n", buffer);
				if (numbytes <= 0){
					epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
					close(fd);
				}
				else{
					http_parse(buffer, fd);
					//send(fd, resp, sizeof(resp), 0);
				}


			}


			
		}
	}

	freeaddrinfo(res);
	close(listen_fd); // Close listening socket
	close(epfd);
	close_logger(); // close the logger

	return 0;
}

