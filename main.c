#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
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
	return strdup("/tmp/kmsd.sock");
}

static int process_message(struct client_message *msg) {
	FILE *f = fopen(msg->file_path, "w+");
	if (f == NULL) {
		return -1;
	}
	fwrite(msg->data, 1, strlen(msg->data), f);
	fclose(f);
	return 0;
}

void signal_handler(int signal) {
	(void)signal;
}

int main(int argc, char *argv[]) {
	const struct sigaction act = {.sa_handler = signal_handler};
	if (sigaction(SIGINT, &act, NULL) || sigaction(SIGTERM, &act, NULL)) {
		perror("sigaction");
		exit(1);
	}

	char *sockpath = get_sock_path();
	unlink(sockpath);
	server_t server = {0};
	if (server_init(&server, sockpath) == -1) {
		perror("server init");
		server_free(&server);
		return -1;
	}

	// FILE *const st = stdout;
	// fprintf(st, "clients: [");
	// for (int i = 0; i < server.client_count; i++) {
	// 	fprintf(st, i == 0 ? "%d" : ", %d", server.client_fds[i]);
	// }
	// fprintf(st, "]\n");
	// exit(0);

	for (;;) {
		const int nfds = epoll_wait(
			server.epoll_fd, server.epoll_events, MAX_EVENTS, EPOLL_TIMEOUT);
		if (nfds == -1) {
			if (errno != EINTR) {
				perror("epoll_wait");
			}
			break;
		}
		for (int i = 0; i < nfds; i++) {
			const struct epoll_event ev = server.epoll_events[i];
			// char *event_names = epoll_events_to_str(ev.events);
			// printf(
			// 	"[epoll event] fd: %d (%s), events: %s\n", ev.data.fd,
			// 	ev.data.fd == server.sock_fd ? "server" : "client",
			// 	event_names);
			// free(event_names);
			if (ev.events & EPOLLIN) {
				if (ev.data.fd == server.sock_fd) {
					if (server_accept_client(&server, &ev) == -1) {
						perror("client accept");
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
							if (process_message(&msg) != 0) {
								perror("process message");
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
					server_remove_client(&server, &ev);
				}
			}
		}
	}

	server_free(&server);
	unlink(sockpath);
	free(sockpath);
	printf("\nBye.\n");
	return 0;
}
