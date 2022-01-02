#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "message.h"

char *get_sock_path() {
	char *sock_path = getenv("SOCKPATH");
	if (sock_path && strlen(sock_path) > 0) {
		return sock_path;
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

bool process_message(struct client_message *msg) {
	FILE *f = fopen(msg->file_path, "w+");
	if (f == NULL) {
		perror("fopen");
		return false;
	}
	fwrite(msg->data, 1, strlen(msg->data), f);
	fclose(f);
	return true;
}

int main(int argc, char *argv[]) {
	int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un server_addr = {0};
	server_addr.sun_family = AF_UNIX;
	char *sock_path = get_sock_path();
	unlink(sock_path);
	memcpy(server_addr.sun_path, sock_path, strlen(sock_path) + 1);
	if (sockfd == -1) {
		perror("socket");
		return 1;
	}
	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
		-1) {
		perror("bind");
		close(sockfd);
		return 1;
	}
	if (listen(sockfd, 50) == -1) {
		perror("listen");
		close(sockfd);
		return 1;
	}

	while (true) {
		struct sockaddr_un client_addr = {0};
		socklen_t addr_size;
		int clientfd =
			accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
		if (clientfd == -1) {
			perror("accept");
			continue;
		}
		char *raw;
		struct client_message msg = {0};
		size_t nread = recv_from_sock(clientfd, &raw);
		if (parse_message(raw, nread, &msg)) {
			printf("-> %s\n", msg.file_path);
			if (!process_message(&msg)) {
				fprintf(stderr, "Failed to process message\n");
			}
			free_message(&msg);
		} else {
			printf("Failed to parse message\n");
		}
		free(raw);
		close(clientfd);
	}

	close(sockfd);
	return 0;
}
