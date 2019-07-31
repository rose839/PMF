#ifndef PMF_SOCKET_H
#define PMF_SOCKET_H

#include "pmf_worker_pool.h"

#define PMF_BACKLOG_DEFAULT 511

extern PMF_ADDRESS_DOMAIN_E pmf_sockets_domain_from_address(char *address);

#endif
