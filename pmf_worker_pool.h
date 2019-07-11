
#ifndef PMF_WORKER_POOL_H
#define PMF_WORKER_POOL_H

#include "pmf_conf.h"
#include "pmf_children.h"
#include "pmf_scoreboard.h"

typedef struct _pmf_worker_pool_s {
	struct _pmf_worker_pool_s *next;
	PMF_WORKER_POOL_CONFIG_S *config;
	int listening_socket_fd;

	/* run time */
	int running_children_num;
	int idle_rate;
	int max_children;
	PMF_CHILD_S *children;
	PMF_SCOREBOARD_S *scoreboard;
}PMF_WORKER_POOL_S;

#endif
