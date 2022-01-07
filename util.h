#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>

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

static int make_fd_nonblocking(const int fd) {
	const int fdflags = fcntl(fd, F_GETFD);
	return fcntl(fd, F_SETFD, fdflags | O_NONBLOCK);
}

#endif
