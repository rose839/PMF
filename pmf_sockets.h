#ifndef PMF_SOCKET_H
#define PMF_SOCKET_H

#include "pmf_worker_pool.h"

#define PMF_BACKLOG_DEFAULT 511

extern PMF_ADDRESS_DOMAIN_E pmf_sockets_domain_from_address(char *address);

static inline int fd_set_blocked(int fd, int blocked) {
	int flags = fcntl(fd, F_GETFL);

	if (flags < 0) {
		return -1;
	}

	if (blocked) {
		flags |= O_NONBLOCK; 
	} else {
		flags &= ~O_NONBLOCK;
	}
	return fcntl(fd, F_SETFL, flags);
}

#endif
