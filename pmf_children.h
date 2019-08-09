
#ifndef PMF_CHILDREN_H
#define PMF_CHILDREN_H

#include <sys/types.h>
#include <unistd.h>
#include "pmf_worker_pool.h"

typedef struct _pmf_worker_pool_s PMF_WORKER_POOL_S;

typedef struct _pmf_child_s {
	struct _pmf_child_s *prev, *next;
	struct timeval start_time;
	PMF_WORKER_POOL_S *wp;
	pid_t pid;
	int scoreboard_index;
}PMF_CHILD_S;

extern int pmf_children_init_main(); 

#endif