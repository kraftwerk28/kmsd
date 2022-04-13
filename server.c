#include "server.h"

int server_init(server_t *server, const char *const sockpath) {
	server->sock_fd = -1;
	server->epoll_fd = -1;
	memset(server->client_fds, 0, sizeof(server->client_fds));
	server->client_count = 0;
	memset(server->epoll_events, 0, sizeof(server->epoll_events));
	if (server_init_unix_socket(server) == -1)
		return -1;
	if (server_init_epoll(server) == -1)
		return -1;
	if (server_bind_listen(server, sockpath))
		return -1;
	return 0;
}

int server_free(server_t *server) {
	for (int i = 0; i < server->client_count; i++) {
		close(server->client_fds[i]);
		epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, server->client_fds[i], NULL);
	}
	if (server->sock_fd != -1) {
		close(server->sock_fd);
		if (server->epoll_fd != -1) {
			epoll_ctl(server->epoll_fd, EPOLL_CTL_DEL, server->sock_fd, NULL);
		}
	}
	if (server->epoll_fd != -1) {
		close(server->epoll_fd);
	}
	return 0;
}

int server_init_unix_socket(server_t *server) {
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		return -1;
	make_fd_nonblocking(fd);
	server->sock_fd = fd;
	return 0;
}

int server_init_epoll(server_t *server) {
	int fd = epoll_create1(0);
	if (fd == -1)
		return -1;
	server->epoll_fd = fd;
	return 0;
}

int server_bind_listen(server_t *server, const char *const sock_path) {
	struct sockaddr_un server_addr = {0};
	server_addr.sun_family = AF_UNIX;
	memcpy(server_addr.sun_path, sock_path, strlen(sock_path) + 1);
	int status = 0;
	// printf("Creating socket at %s\n", sock_path);
	// FIXME: fchmod doesn't set write permissions
	// if (fchmod(server->sock_fd, 0766) == -1) {
	// 	perror("fchmod");
	// 	return -1;
	// }
	const mode_t old_mask = umask(0000);
	if (bind(
			server->sock_fd, (struct sockaddr *)&server_addr,
			sizeof(server_addr)) < 0) {
		return -1;
	}
	umask(old_mask);
	if ((status = listen(server->sock_fd, MAX_BACKLOG)) == -1)
		return status;
	struct epoll_event event = {0};
	event.events = EPOLLET | EPOLLIN;
	event.data.fd = server->sock_fd;
	epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, server->sock_fd, &event);
	return status;
}

int server_accept_client(
	server_t *server, const struct epoll_event *epoll_event) {
	struct sockaddr_un client_addr = {0};
	socklen_t client_addr_size = sizeof(client_addr);
	int client_fd = accept(
		server->sock_fd, (struct sockaddr *)&client_addr, &client_addr_size);
	if (client_fd == -1) {
		return -1;
	}
	make_fd_nonblocking(client_fd);
	return server_add_epoll_fd(server, client_fd);
}

// int server_remove_client(server_t *server, const struct epoll_event *event) {
// 	int fd_index = 0;
// 	int fd = event->data.fd;
// 	while (server->client_fds[fd_index] != fd &&
// 		   fd_index < server->client_count) {
// 		fd_index++;
// 	}
// 	if (fd_index >= server->client_count)
// 		return -1;
// 	if (fd_index < MAX_CLIENTS - 1) {
// 		memmove(
// 			&server->client_fds[fd_index], &server->client_fds[fd_index + 1],
// 			MAX_CLIENTS - fd_index - 1);
// 	}
// 	server->client_count--;
// 	close(fd);
// 	return 0;
// }

int server_remove_epoll_fd(server_t *server, int fd) {
	int idx = 0;
	for (; idx < server->client_count; idx++) {
		if (server->client_fds[idx] == fd) {
			break;
		}
	}
	if (idx >= server->client_count) {
		return -1;
	}
	memmove(
		&server->client_fds[idx], &server->client_fds[idx + 1],
		server->client_count - idx - 1);
	server->client_count--;
	return 0;
	// int fd_index = 0;
	// int fd = event->data.fd;
	// while (server->client_fds[fd_index] != fd &&
	// 	   fd_index < server->client_count) {
	// 	fd_index++;
	// }
	// if (fd_index >= server->client_count)
	// 	return -1;
	// if (fd_index < MAX_CLIENTS - 1) {
	// 	memmove(
	// 		&server->client_fds[fd_index], &server->client_fds[fd_index + 1],
	// 		MAX_CLIENTS - fd_index - 1);
	// }
	// server->client_count--;
	// close(fd);
	// return 0;
}

int server_add_epoll_fd(server_t *server, int fd) {
	server->client_fds[server->client_count] = fd;
	server->client_count++;
	struct epoll_event client_event = {
		.events = EPOLLET | EPOLLIN | EPOLLRDHUP,
		.data.fd = fd,
	};
	return epoll_ctl(server->epoll_fd, EPOLL_CTL_ADD, fd, &client_event);
}
