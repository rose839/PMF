
#ifndef PMF_CHILDREN_H
#define PMF_CHILDREN_H

#include <sys/types.h>
#include <unistd.h>

typedef struct _pmf_child_s {
	struct _pmf_child_s *prev, *next;
	struct timeval start_time;
	struct fpm_worker_pool_s *wp;
	pid_t pid;
	int scoreboard_index;
}PMF_CHILD_S;

#endif