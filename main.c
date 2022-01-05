#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "message.h"
#include "server.h"

#define EPOLL_TIMEOUT -1
#define MAX_EVENTS 16

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

// Helper to append event name to event list
#define ev(name)                                                               \
	do {                                                                       \
		if (events & name)                                                     \
			namelist[i++] = #name;                                             \
	} while (0);

static char *epoll_events_to_str(uint32_t events) {
	char *namelist[16];
	int i = 0;
	ev(EPOLLIN);
	ev(EPOLLOUT);
	ev(EPOLLRDHUP);
	ev(EPOLLPRI);
	ev(EPOLLERR);
	ev(EPOLLHUP);
	ev(EPOLLET);
	ev(EPOLLONESHOT);
	ev(EPOLLWAKEUP);
	ev(EPOLLEXCLUSIVE);
	char *buf = calloc(64, 1);
	strncat(buf, namelist[0], 64 - 1);
	for (int j = 1; j < i; j++) {
		strncat(buf, ", ", 64 - 1);
		strncat(buf, namelist[j], 64 - 1);
	}
	return buf;
}

static void signal_handler(int sig) {
}

static volatile int nclients = 0;

int main(int argc, char *argv[]) {
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		perror("signal");
		exit(1);
	}
	if (signal(SIGTERM, signal_handler) == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	char *sockpath = get_sock_path();
	unlink(sockpath);
	server_t server = {0};
	server_init_unix_socket(&server);
	server_bind_listen(&server, sockpath);

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
			// char *event_names = epoll_events_to_str(ev.events);
			// printf(
			// 	"[epoll event] fd: %d (%s), events: %s\n", ev.data.fd,
			// 	ev.data.fd == server.sock_fd ? "server" : "client",
			// 	event_names);
			// free(event_names);
			if (ev.events & EPOLLIN) {
				if (ev.data.fd == server.sock_fd) {
					struct sockaddr_un client_addr = {0};
					socklen_t client_addr_size = sizeof(client_addr);
					int client_fd = accept(
						server.sock_fd, (struct sockaddr *)&client_addr,
						&client_addr_size);
					nclients++;
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
					} else {
						// printf("read %zu bytes from client\n", nread);
						if (parse_message(raw_msg, nread, &msg)) {
							// printf("> \"%s\"\n", msg.file_path);
							if (!process_message(&msg)) {
								fprintf(stderr, "Failed to process message\n");
							}
							free_message(&msg);
						} else {
							printf(
								"Failed to parse message: [%.*s]\n", (int)nread,
								raw_msg);
						}
						free(raw_msg);
					}
				}
			}
			if (ev.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				if (ev.data.fd == server.sock_fd) {
					// unreachable
				} else {
					nclients--;
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ev.data.fd, NULL);
					close(ev.data.fd);
				}
			}
		}
	}
	close(epoll_fd);
	close(server.sock_fd);
	unlink(sockpath);
	free(sockpath);
	printf("\nBye. Remaining clients: %d\n", nclients);
	return 0;
}
