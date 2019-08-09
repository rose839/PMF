#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "pmf_worker_pool.h"
#include "pmf_array.h"
#include "pmf_unix.h"
#include "pmf_log.h"
#include "pmf_scoreboard.h"

struct listening_socket_s {
	int refcount;
	int sock;
	int type;
	char *key;
};

static PMF_ARRAY_S sockets_list;

enum { PMF_GET_USE_SOCKET = 1, PMF_STORE_SOCKET = 2, PMF_STORE_USE_SOCKET = 3 };


PMF_ADDRESS_DOMAIN_E pmf_sockets_domain_from_address(char *address) {
	if (strchr(address, ':') ||
		strlen(address) == strspn(address, "0123456789")) {
		return PMF_AF_INET;
	}

	return PMF_AF_UNIX;
}

static void *pmf_get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static int pmf_get_in_port(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return ntohs(((struct sockaddr_in*)sa)->sin_port);
    }

    return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
}

static int pmf_sockets_hash_op(int sock, struct sockaddr *sa, char *key, int type, int op) {
	bool key_need_free = false;
	int ret_sock = -1;

	if (key == NULL) {
		switch (type) {
			case PMF_AF_INET : {
				if (NULL == (key = malloc(INET6_ADDRSTRLEN+10))) {
					return -1;
				}
				key_need_free = true;
				inet_ntop(sa->sa_family, pmf_get_in_addr(sa), key, INET6_ADDRSTRLEN);
				sprintf(key+strlen(key), ":%d", pmf_get_in_port(sa));
				break;
			}

			case PMF_AF_UNIX : {
				struct sockaddr_un *sa_un = (struct sockaddr_un *) sa;
				if (NULL == (key = malloc(strlen(sa_un->sun_path) + 1))) {
					return -1;
				}
				key_need_free = true;
				strcpy(key, sa_un->sun_path);
				break;
			}

			default :
				return -1;
		}
	}

	switch (op) {

		case PMF_GET_USE_SOCKET :
		{
			unsigned i;
			struct listening_socket_s *ls = sockets_list.data;

			for (i = 0; i < sockets_list.used_num; i++, ls++) {
				if (!strcmp(ls->key, key)) {
					++ls->refcount;
					ret_sock = ls->sock;
					break;
				}
			}
			break;
		}

		case PMF_STORE_SOCKET :			/* inherited socket */
		case PMF_STORE_USE_SOCKET :		/* just created */
		{
			struct listening_socket_s *ls;

			ls = pmf_array_push(&sockets_list);
			if (!ls) {
				break;
			}

			if (op == PMF_STORE_SOCKET) {
				ls->refcount = 0;
			} else {
				ls->refcount = 1;
			}
			ls->type = type;
			ls->sock = sock;
			ls->key = strdup(key);

			ret_sock = 0;
		}
	}

	if (key_need_free) {
		free(key);
	}
	
	return ret_sock;
}

int pmf_socket_unix_test_connect(struct sockaddr_un *sock, size_t socklen) {
	int fd;

	if (!sock || sock->sun_family != AF_UNIX) {
		return -1;
	}

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		return -1;
	}

	if (connect(fd, (struct sockaddr *)sock, socklen) == -1) {
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int pmf_sockets_new_listening_socket(PMF_WORKER_POOL_S *wp, struct sockaddr *sa, int socklen) {
	int flags = 1;
	int sock;
	mode_t saved_umask = 0;

	sock = socket(sa->sa_family, SOCK_STREAM, 0);

	if (0 > sock) {
		plog(PLOG_SYSERROR, "failed to create new listening socket: socket()");
		return -1;
	}

	if (0 > setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags))) {
		plog(PLOG_WARNING, "failed to change socket attribute");
	}

	if (wp->listen_address_domain == PMF_AF_UNIX) {
		if (pmf_socket_unix_test_connect((struct sockaddr_un *)sa, socklen) == 0) {
			plog(PLOG_ERROR, "An another FPM instance seems to already listen on %s", ((struct sockaddr_un *) sa)->sun_path);
			close(sock);
			return -1;
		}
		unlink( ((struct sockaddr_un *) sa)->sun_path);
		saved_umask = umask(0777 ^ wp->socket_mode);
	}

	if (0 > bind(sock, sa, socklen)) {
		plog(PLOG_SYSERROR, "unable to bind listening socket for address '%s'", wp->config->listen_address);
		if (wp->listen_address_domain == PMF_AF_UNIX) {
			umask(saved_umask);
		}
		close(sock);
		return -1;
	}

	if (wp->listen_address_domain == PMF_AF_UNIX) {
		char *path = ((struct sockaddr_un *) sa)->sun_path;

		umask(saved_umask);

		if (0 > pmf_unix_set_socket_premissions(wp, path)) {
			close(sock);
			return -1;
		}
	}

	if (0 > listen(sock, wp->config->listen_backlog)) {
		plog(PLOG_SYSERROR, "failed to listen to address '%s'", wp->config->listen_address);
		close(sock);
		return -1;
	}

	return sock;
}

static int pmf_sockets_get_listening_socket(PMF_WORKER_POOL_S *wp, struct sockaddr *sa, int socklen) {
	int sock;

	sock = pmf_sockets_hash_op(0, sa, 0, wp->listen_address_domain, PMF_GET_USE_SOCKET);
	if (sock >= 0) {
		return sock;
	}

	sock = pmf_sockets_new_listening_socket(wp, sa, socklen);
	pmf_sockets_hash_op(sock, sa, 0, wp->listen_address_domain, PMF_STORE_USE_SOCKET);

	return sock;
}

static int pmf_socket_af_inet_socket_by_addr(PMF_WORKER_POOL_S *wp, const char *addr, const char *port) {
	struct addrinfo hints, *servinfo, *p;
	char tmpbuf[INET6_ADDRSTRLEN];
	int status;
	int sock = -1;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((status = getaddrinfo(addr, port, &hints, &servinfo)) != 0) {
		plog(PLOG_ERROR, "getaddrinfo: %s\n", gai_strerror(status));
		return -1;
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		inet_ntop(p->ai_family, pmf_get_in_addr(p->ai_addr), tmpbuf, INET6_ADDRSTRLEN);
		if (sock < 0) {
			if ((sock = pmf_sockets_get_listening_socket(wp, p->ai_addr, p->ai_addrlen)) != -1) {
				plog(PLOG_DEBUG, "Found address for %s, socket opened on %s", addr, tmpbuf);
			}
		} else {
			plog(PLOG_WARNING, "Found multiple addresses for %s, %s ignored", addr, tmpbuf);
		}
	}

	freeaddrinfo(servinfo);

	return sock;
}

static int pmf_socket_af_inet_listening_socket(PMF_WORKER_POOL_S *wp) {
	char *dup_address = strdup(wp->config->listen_address);
	char *port_str = strrchr(dup_address, ':');
	char *addr = NULL;
	int addr_len;
	int port = 0;
	int sock = -1;

	if (port_str) { /* this is host:port pair */
		*port_str++ = '\0';
		port = atoi(port_str);
		addr = dup_address;

		/* strip brackets from address for getaddrinfo */
		addr_len = strlen(addr);
		if (addr[0] == '[' && addr[addr_len - 1] == ']') {
			addr[addr_len - 1] = '\0';
			addr++;
		}
	} else if (strlen(dup_address) == strspn(dup_address, "0123456789")) { /* this is port */
		port = atoi(dup_address);
		port_str = dup_address;
	}

	if (port == 0) {
		plog(PLOG_ERROR, "invalid port value '%s'", port_str);
		return -1;
	}

	if (addr) {
		/* Bind a specific address */
		sock = pmf_socket_af_inet_socket_by_addr(wp, addr, port_str);
	} else {
		/* Bind ANYADDR
		 *
		 * Try "::" first as that covers IPv6 ANYADDR and mapped IPv4 ANYADDR
		 * silencing warnings since failure is an option
		 *
		 * If that fails (because AF_INET6 is unsupported) retry with 0.0.0.0
		 */
		int old_level = plog_set_level(PLOG_ALERT);
		sock = pmf_socket_af_inet_socket_by_addr(wp, "::", port_str);
		plog_set_level(old_level);

		if (sock < 0) {
			plog(PLOG_NOTICE, "Failed implicitly binding to ::, retrying with 0.0.0.0");
			sock = pmf_socket_af_inet_socket_by_addr(wp, "0.0.0.0", port_str);
		}
	}

	free(dup_address);

	return sock;
}

static int pmf_socket_af_unix_listening_socket(PMF_WORKER_POOL_S *wp) {
	struct sockaddr_un sa_un;

	memset(&sa_un, 0, sizeof(sa_un));
	strncpy(sa_un.sun_path, wp->config->listen_address, sizeof(sa_un.sun_path));
	sa_un.sun_family = AF_UNIX;
	return pmf_sockets_get_listening_socket(wp, (struct sockaddr *) &sa_un, sizeof(struct sockaddr_un));
}

int pmf_socket_get_listening_queue(int sock, unsigned *cur_lq, unsigned *max_lq) {
	struct tcp_info info;
	socklen_t len = sizeof(info);

	if (0 > getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, &len)) {
		plog(PLOG_SYSERROR, "failed to retrieve TCP_INFO for socket");
		return -1;
	}

	/* kernel >= 2.6.24 return non-zero here, that means operation is supported */
	if (info.tcpi_sacked == 0) {
		return -1;
	}

	if (cur_lq) {
		*cur_lq = info.tcpi_unacked;
	}

	if (max_lq) {
		*max_lq = info.tcpi_sacked;
	}

	return 0;
}

int pmf_sockets_init_main() {
	unsigned i, lq_len;
	PMF_WORKER_POOL_S *wp;
	struct listening_socket_s *ls;

	if (0 == pmf_array_init(&sockets_list, sizeof(struct listening_socket_s), 10)) {
		return -1;
	}

	/* create all required sockets */
	for (wp = pmf_worker_all_pools; wp; wp = wp->next) {
		switch (wp->listen_address_domain) {
			case PMF_AF_INET :
				wp->listening_socket_fd = pmf_socket_af_inet_listening_socket(wp);
				break;
			case PMF_AF_UNIX :
				if (0 > pmf_unix_resolve_socket_premissions(wp)) {
					return -1;
				}
				wp->listening_socket_fd = pmf_socket_af_unix_listening_socket(wp);
				break;
		}

		if (wp->listening_socket_fd == -1) {
			return -1;
		}

	if (wp->listen_address_domain == PMF_AF_INET && pmf_socket_get_listening_queue(wp->listening_socket_fd, NULL, &lq_len) >= 0) {
			pmf_scoreboard_update(-1, -1, -1, (int)lq_len, -1, -1, 0, PMF_SCOREBOARD_ACTION_SET, wp->scoreboard);
		}
	}

	/* close unused sockets that was inherited */
	ls = sockets_list.data;

	for (i = 0; i < sockets_list.used_num; ) {
		if (ls->refcount == 0) {
			close(ls->sock);
			if (ls->type == PMF_AF_UNIX) {
				unlink(ls->key);
			}
			free(ls->key);
			pmf_array_remove_item(&sockets_list, i);
		} else {
			++i;
			++ls;
		}
	}
}

