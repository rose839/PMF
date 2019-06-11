
#include "pmf_log.h"
#include "pmf_events.h"
#include "plog.h"

#define PMF_CONF_FILE "/etc/pmf.conf"

static int pmf_conf_load_conf_file(char *filename) {
	int ret = 0;

	return ret;
}

int pmf_conf_init_main() {
	int ret;

	ret = pmf_conf_load_conf_file(PMF_CONF_FILE);
	if (0 > ret) {
		plog(PLOG_ERROR, "failed to load configuration file '%s'", PMF_CONF_FILE);
		return -1;
	}

	return ret;
}
