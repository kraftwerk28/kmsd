#ifndef _SERVER_H_
#define _SERVER_H_

#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#define MAX_BACKLOG 50

typedef struct server {
	int sock_fd, epoll_fd;
} server_t;

int server_init_unix_socket(server_t *server);
int server_bind_listen(server_t *server, const char *sock_path);

static int make_fd_nonblocking(const int fd) {
	const int fdflags = fcntl(fd, F_GETFD);
	return fcntl(fd, F_SETFD, fdflags | O_NONBLOCK);
}

int server_init_unix_socket(server_t *server) {
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		return -1;
	}
	make_fd_nonblocking(fd);
	server->sock_fd = fd;
	return 0;
}

int server_bind_listen(server_t *server, const char *sock_path) {
	struct sockaddr_un server_addr = {0};
	server_addr.sun_family = AF_UNIX;
	memcpy(server_addr.sun_path, sock_path, strlen(sock_path) + 1);
	int status = 0;
	printf("Creating socket at %s\n", sock_path);
	// FIXME: fchmod doesn't set write permissions
	// if (fchmod(server->sock_fd, 0766) == -1) {
	// 	perror("fchmod");
	// 	return -1;
	// }
	const mode_t old_mask = umask(0000);
	if ((status = bind(
			 server->sock_fd, (struct sockaddr *)&server_addr,
			 (socklen_t)sizeof(server_addr))) == -1) {
		perror("bind");
		return status;
	}
	umask(old_mask);
	if ((status = listen(server->sock_fd, MAX_BACKLOG)) == -1) {
		perror("listen");
		return status;
	}
	return status;
}

#endif
