
#ifndef PMF_H
#define PMF_H

/* structure in every process */
typedef struct _pmf_global_s {
	pid_t parent_pid;
	int is_child;
	int listening_socket; /* this child */
	int max_requests;     /* this child */
}PMF_GLOBAL_S;

#endif
