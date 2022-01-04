#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "message.h"

#define MAX_BACKLOG 64
#define EPOLL_TIMEOUT -1
#define MAX_EVENTS 16

static int make_fd_nonblocking(const int fd);

struct server {
	int sock_fd, epoll_fd;
};
typedef struct server server_t;

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
	unlink(sock_path);
	memcpy(server_addr.sun_path, sock_path, strlen(sock_path) + 1);
	int status = 0;
	if ((status = bind(
			 server->sock_fd, (struct sockaddr *)&server_addr,
			 (socklen_t)sizeof(server_addr))) == -1) {
		// perror("bind");
		return status;
	}
	if ((status = listen(server->sock_fd, MAX_BACKLOG)) == -1) {
		// perror("listen");
		return status;
	}
	return status;
}

static char *get_sock_path() {
	char *sock_path = getenv("SOCKPATH");
	if (sock_path && strlen(sock_path) > 0) {
		return strdup(sock_path);
	}
	const char *dir = getenv("XDG_RUNTIME_DIR");
	if (!dir) {
		dir = "/tmp";
	}
	const size_t buf_size = 256;
	char *buf = calloc(buf_size, 1);
	if (snprintf(buf, buf_size, "%s/kmsd.sock", dir) == buf_size) {
		return NULL;
	}
	return buf;
}

static bool process_message(struct client_message *msg) {
	FILE *f = fopen(msg->file_path, "w+");
	if (f == NULL) {
		perror("fopen");
		return false;
	}
	fwrite(msg->data, 1, strlen(msg->data), f);
	fclose(f);
	return true;
}

static char *epoll_events_to_str(uint32_t events) {
#define ev(name)                                                               \
	do {                                                                       \
		if (events & name)                                                     \
			namelist[i++] = #name;                                             \
	} while (0);
	char *namelist[8];
	int i = 0;
	ev(EPOLLOUT);
	ev(EPOLLIN);
	ev(EPOLLET);
	ev(EPOLLERR);
	ev(EPOLLHUP);
	ev(EPOLLRDHUP);
	char *buf = calloc(64, 1);
	strncat(buf, namelist[0], 64 - 1);
	for (int j = 1; j < i; j++) {
		strncat(buf, ", ", 64 - 1);
		strncat(buf, namelist[j], 64 - 1);
	}
	return buf;
}

static int make_fd_nonblocking(const int fd) {
	const int fdflags = fcntl(fd, F_GETFD);
	return fcntl(fd, F_SETFD, fdflags | O_NONBLOCK);
}

static void signal_handler(int sig) {
}

int main(int argc, char *argv[]) {
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	server_t server = {0};
	server_init_unix_socket(&server);
	char *sockpath = get_sock_path();
	server_bind_listen(&server, sockpath);
	free(sockpath);

	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		perror("epoll_create1");
		exit(1);
	}
	struct epoll_event epoll_events[MAX_EVENTS] = {0};
	struct epoll_event event = {0};
	event.events = EPOLLET | EPOLLIN;
	event.data.fd = server.sock_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server.sock_fd, &event) == -1) {
		perror("epoll_ctl");
		exit(1);
	}
	for (;;) {
		const int nfds =
			epoll_wait(epoll_fd, epoll_events, MAX_EVENTS, EPOLL_TIMEOUT);
		if (nfds == -1) {
			if (errno != EINTR) {
				perror("epoll_wait");
			}
			break;
		}
		for (int i = 0; i < nfds; i++) {
			const struct epoll_event ev = epoll_events[i];
			char *event_names = epoll_events_to_str(ev.events);
			// printf(
			// 	"[epoll event] fd: %d (%s), events: %s\n", ev.data.fd,
			// 	ev.data.fd == server_fd ? "server" : "client", event_names);
			free(event_names);
			if (ev.events & EPOLLIN) {
				if (ev.data.fd == server.sock_fd) {
					struct sockaddr_un client_addr = {0};
					socklen_t client_addr_size = sizeof(client_addr);
					int client_fd = accept(
						server.sock_fd, (struct sockaddr *)&client_addr,
						&client_addr_size);
					if (client_fd == -1) {
						perror("accept");
						continue;
					}
					make_fd_nonblocking(client_fd);
					struct epoll_event client_event = {0};
					client_event.events = EPOLLET | EPOLLIN | EPOLLRDHUP;
					client_event.data.fd = client_fd;
					if (epoll_ctl(
							epoll_fd, EPOLL_CTL_ADD, client_fd,
							&client_event) == -1) {
						perror("epoll_ctl (client)");
					}
					continue;
				} else {
					char *raw_msg = NULL;
					struct client_message msg;
					size_t nread = recv_from_sock(ev.data.fd, &raw_msg);
					if (nread == 0) {
						free(raw_msg);
						continue;
					}
					if (parse_message(raw_msg, nread, &msg)) {
						if (!process_message(&msg)) {
							fprintf(stderr, "Failed to process message\n");
						}
						free_message(&msg);
					} else {
						printf("Failed to parse message\n");
					}
					free(raw_msg);
				}
			}
			if (ev.events & (EPOLLRDHUP | EPOLLHUP)) {
				if (ev.data.fd == server.sock_fd) {
					// Sure it's unreachable!
				} else {
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ev.data.fd, NULL);
					close(ev.data.fd);
				}
			}
		}
	}
	close(epoll_fd);
	close(server.sock_fd);
	printf("\nBye.\n");
	return 0;
}
