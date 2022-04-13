#ifndef _SERVER_H_
#define _SERVER_H_

#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "util.h"

#define MAX_BACKLOG 50
#define MAX_EPOLL_EVENTS 16
#define MAX_CLIENTS 1024

struct server {
	int sock_fd;
	int epoll_fd;
	int client_fds[MAX_CLIENTS];
	int client_count;
	struct epoll_event epoll_events[MAX_EPOLL_EVENTS];
};

typedef struct server server_t;

int server_init(server_t *server, const char *const sock_path);
int server_free(server_t *server);
int server_init_unix_socket(server_t *server);
int server_init_epoll(server_t *server);
int server_bind_listen(server_t *server, const char *sockpath);
int server_accept_client(
	server_t *server, const struct epoll_event *epoll_event);
int server_remove_epoll_fd(server_t *server, int fd);
int server_add_epoll_fd(server_t *server, int fd);

#endif
