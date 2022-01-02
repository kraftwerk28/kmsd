#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_CAP 128

static size_t recv_from_sock(const int sockfd, char **message) {
	size_t total_read = 0;
	*message = NULL;
	char buf[BUF_CAP];
	while (true) {
		ssize_t nread = read(sockfd, buf, BUF_CAP);
		total_read += nread;
		if (message == NULL) {
			*message = malloc(nread);
			memcpy(*message, buf, nread);
		} else {
			*message = realloc(*message, total_read);
			memcpy(*message + total_read - nread, buf, nread);
		}
		if (nread < BUF_CAP) {
			break;
		}
	}
	return total_read;
}

struct client_message {
	char *file_path;
	char *data;
};

static bool parse_message(
	char *raw, size_t message_len, struct client_message *out_message) {
	size_t file_path_len = 0;
	for (; file_path_len < message_len; file_path_len++) {
		if (raw[file_path_len] == '\n')
			break;
	}
	if (file_path_len >= message_len - 1) {
		return false;
	}
	size_t data_len = message_len - file_path_len - 1;
	out_message->file_path = strndup(raw, file_path_len);
	out_message->file_path[file_path_len] = 0;
	out_message->data = strndup(raw + file_path_len + 1, data_len);
	out_message->data[data_len] = 0;
	return true;
}

void free_message(struct client_message *msg) {
	free(msg->file_path);
	free(msg->data);
}

#endif
