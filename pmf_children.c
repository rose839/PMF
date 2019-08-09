#include <stdlib.h>
#include <string.h>
#include "pmf_conf.h"

static time_t *last_faults;

int pmf_children_init_main() {
	if (pmf_global_config.emergency_restart_threshold &&
		pmf_global_config.emergency_restart_interval) {

		last_faults = malloc(sizeof(time_t) * pmf_global_config.emergency_restart_threshold);

		if (!last_faults) {
			return -1;
		}

		memset(last_faults, 0, sizeof(time_t) * pmf_global_config.emergency_restart_threshold);
	}

	return 0;
}

