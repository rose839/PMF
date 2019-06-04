
#include "pmf.h"
#include "pmf_conf.h"

PMF_GLOBAL_S pmf_globals = {
	.parent_pid = 0;
	.listening_socket = 0;
	.max_requests = 0;
};

int pmf_init() {
	return 0;
}

int pmf_run() {
	return 0;
}

