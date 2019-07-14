#include <sys/types.h>
#include <unistd.h>
#include "pmf.h"
#include "pmf_conf.h"
#include "pmf_stdio.h"
#include "pmf_log.h"

PMF_GLOBAL_S pmf_globals = {
	.parent_pid = 0,
	.argc = 0,
	.argv = NULL,
	.running_children_num = 0,
	.error_log_fd = 0,
	.log_level = 0,
	.listening_socket = 0,
	.max_requests = 0,
};

int pmf_init() {
	if (0 > pmf_stdio_init_main() ||
		0 > pmf_conf_init_main()) {

		plog(PLOG_ERROR, "PMF initialization failed");
		return -1;
	}

	return 0;
}

int pmf_run() {
	return 0;
}

