#include "pmf_conf.h"
#include "pmf_log.h"

PMF_WORKER_POOL_S *pmf_worker_all_pools;

PMF_WORKER_POOL_S *pmf_worker_pool_alloc() {
	PMF_WORKER_POOL_S *new_pool = NULL;

	new_pool = malloc(sizeof(PMF_WORKER_POOL_S));
	if (NULL == new_pool) {
		plog(PLOG_ERROR, "alloc worker pool failed.");
		return NULL;
	}

	memset(new_pool, 0, sizeof(PMF_WORKER_POOL_S));

	new_pool->idle_rate = 1;
	return new_pool;
}

void pmf_worker_pool_free(PMF_WORKER_POOL_S *wp) {
	if (wp->config) {
		free(wp->config);
	}

	free(wp);
}

int pmf_worker_pool_init_main() {

	return 0;
}

