#include <string.h>
#include "pmf_worker_pool.h"

PMF_ADDRESS_DOMAIN_E pmf_sockets_domain_from_address(char *address) {
	if (strchr(address, ':') ||
		strlen(address) == strspn(address, "0123456789")) {
		return PMF_AF_INET;
	}

	return PMF_AF_UNIX;
}

