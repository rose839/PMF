
#ifndef PMF_WORKER_POOL_H
#define PMF_WORKER_POOL_H

#include "pmf_conf.h"
#include "pmf_children.h"
#include "pmf_scoreboard.h"

typedef enum _pmf_address_domain_e {
	PMF_AF_UNIX = 1,
	PMF_AF_INET = 2
}PMF_ADDRESS_DOMAIN_E;

typedef struct _pmf_worker_pool_s {
	struct _pmf_worker_pool_s *next;
	PMF_WORKER_POOL_CONFIG_S *config;
	int listening_socket_fd;
	PMF_ADDRESS_DOMAIN_E listen_address_domain;
	char *user, *home;
	int set_uid, set_gid; // config uid and gid
	int socket_uid, socket_gid, socket_mode;

	/* run time */
	int running_children_num;
	int idle_rate;
	int max_children;
	PMF_CHILD_S *children;
	PMF_SCOREBOARD_S *scoreboard;
}PMF_WORKER_POOL_S;

extern PMF_WORKER_POOL_S *pmf_worker_all_pools;

extern PMF_WORKER_POOL_S *pmf_worker_pool_alloc();
extern void pmf_worker_pool_free(PMF_WORKER_POOL_S *wp);
extern int pmf_worker_pool_init_main();

#endif
