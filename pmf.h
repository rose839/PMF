
#ifndef PMF_H
#define PMF_H

#include <sys/types.h>
#include <unistd.h>

/* structure in every process */
typedef struct _pmf_global_s {
	pid_t parent_pid;
	int is_child;
	int listening_socket; /* this child */
	int max_requests;     /* this child */
}PMF_GLOBAL_S;

extern PMF_GLOBAL_S pmf_globals;

#endif
