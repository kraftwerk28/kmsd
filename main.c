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
	int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_fd == -1) {
		perror("socket");
		return 1;
	}
	make_fd_nonblocking(server_fd);
	struct sockaddr_un server_addr = {0};
	server_addr.sun_family = AF_UNIX;
	char *sock_path = get_sock_path();
	unlink(sock_path);
	memcpy(server_addr.sun_path, sock_path, strlen(sock_path) + 1);
	free(sock_path);
	if (bind(
			server_fd, (struct sockaddr *)&server_addr,
			(socklen_t)sizeof(server_addr)) == -1) {
		perror("bind");
		close(server_fd);
		return 1;
	}
	if (listen(server_fd, MAX_BACKLOG) == -1) {
		perror("listen");
		close(server_fd);
		return 1;
	}

	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		perror("epoll_create1");
		exit(1);
	}
	struct epoll_event epoll_events[MAX_EVENTS] = {0};
	struct epoll_event event = {0};
	event.events = EPOLLET | EPOLLIN | EPOLLOUT;
	event.data.fd = server_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
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
		// printf("--- epoll_wait (nfds = %d) ---\n", nfds);
		for (int i = 0; i < nfds; i++) {
			const struct epoll_event ev = epoll_events[i];
			// char *names = epoll_events_to_str(ev.events);
			// printf("i: %d; events: %s; fd: %d\n", i, names, ev.data.fd);
			// free(names);
			if (ev.events & EPOLLIN && ev.data.fd == server_fd) {
				struct sockaddr_un client_addr = {0};
				socklen_t client_addr_size = sizeof(client_addr);
				int client_fd = accept(
					server_fd, (struct sockaddr *)&client_addr,
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
						epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) ==
					-1) {
					perror("epoll_ctl (client)");
				}
				continue;
			}
			if ((ev.events & EPOLLIN) && (ev.data.fd != server_fd)) {
				char *raw_msg = NULL;
				struct client_message msg;
				size_t nread = recv_from_sock(ev.data.fd, &raw_msg);
				if (parse_message(raw_msg, nread, &msg)) {
					printf("-> %s\n", msg.file_path);
					if (!process_message(&msg)) {
						fprintf(stderr, "Failed to process message\n");
					}
					free_message(&msg);
				} else {
					printf("Failed to parse message\n");
				}
				free(raw_msg);
			}
			if ((ev.events & (EPOLLRDHUP | EPOLLHUP)) &&
				(ev.data.fd != server_fd)) {
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ev.data.fd, NULL);
				close(ev.data.fd);
			}
		}
	}

	// while (true) {
	// 	struct sockaddr_un client_addr = {0};
	// 	socklen_t addr_size;
	// 	int clientfd =
	// 		accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
	// 	if (clientfd == -1) {
	// 		perror("accept");
	// 		continue;
	// 	}
	// 	char *raw;
	// 	struct client_message msg = {0};
	// 	size_t nread = recv_from_sock(clientfd, &raw);
	// 	if (parse_message(raw, nread, &msg)) {
	// 		printf("-> %s\n", msg.file_path);
	// 		if (!process_message(&msg)) {
	// 			fprintf(stderr, "Failed to process message\n");
	// 		}
	// 		free_message(&msg);
	// 	} else {
	// 		printf("Failed to parse message\n");
	// 	}
	// 	free(raw);
	// 	close(clientfd);
	// }

	close(epoll_fd);
	close(server_fd);
	printf("Sockets closed\n");
	return 0;
}
