#ifndef PMF_H
#define PMF_H

#include <sys/types.h>
#include <unistd.h>

/* structure in every process */
typedef struct _pmf_global_s {
	pid_t parent_pid;
	int running_children_num;
	int error_log_fd;
	int log_level;
	int is_child;
	int listening_socket; /* this child */
	int max_requests;     /* this child */
	int send_config_pipe[2];
}PMF_GLOBAL_S;

extern PMF_GLOBAL_S pmf_globals;

extern int pmf_init();
extern int pmf_run();

#endif
