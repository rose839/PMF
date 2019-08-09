#include "string.h"
#include "pmf_scoreboard.h"
#include "pmf_atomic.h"
#include "pmf_log.h"
#include "pmf_worker_pool.h"
#include "pmf_shm.h"

static PMF_SCOREBOARD_S *pmf_scoreboard = NULL;
static int pmf_scoreboard_i = -1;

void pmf_scoreboard_update(int idle, int active, int lq, int lq_len, int requests, int max_children_reached, int slow_rq, int action, PMF_SCOREBOARD_S *scoreboard) {
	if (!scoreboard) {
		scoreboard = pmf_scoreboard;
	}
	if (!scoreboard) {
		plog(PLOG_WARNING, "Unable to update scoreboard: the SHM has not been found");
		return;
	}

	pmf_spinlock(&scoreboard->lock, 0);
	if (action == PMF_SCOREBOARD_ACTION_SET) {
		if (idle >= 0) {
			scoreboard->idle = idle;
		}
		if (active >= 0) {
			scoreboard->active = active;
		}
		if (lq >= 0) {
			scoreboard->lq = lq;
		}
		if (lq_len >= 0) {
			scoreboard->lq_len = lq_len;
		}

		if (scoreboard->lq > scoreboard->lq_max) {
			scoreboard->lq_max = scoreboard->lq;
		}
		
		if (requests >= 0) {
			scoreboard->requests = requests;
		}

		if (max_children_reached >= 0) {
			scoreboard->max_children_reached = max_children_reached;
		}
		if (slow_rq > 0) {
			scoreboard->slow_rq = slow_rq;
		}
	} else {
		if (scoreboard->idle + idle > 0) {
			scoreboard->idle += idle;
		} else {
			scoreboard->idle = 0;
		}

		if (scoreboard->active + active > 0) {
			scoreboard->active += active;
		} else {
			scoreboard->active = 0;
		}

		if (scoreboard->requests + requests > 0) {
			scoreboard->requests += requests;
		} else {
			scoreboard->requests = 0;
		}

		if (scoreboard->max_children_reached + max_children_reached > 0) {
			scoreboard->max_children_reached += max_children_reached;
		} else {
			scoreboard->max_children_reached = 0;
		}

		if (scoreboard->slow_rq + slow_rq > 0) {
			scoreboard->slow_rq += slow_rq;
		} else {
			scoreboard->slow_rq = 0;
		}
	}

	if (scoreboard->active > scoreboard->active_max) {
		scoreboard->active_max = scoreboard->active;
	}

	pmf_unlock(scoreboard->lock);
}

int pmf_scoreboard_init_main() {
	PMF_WORKER_POOL_S *wp;

	for (wp = pmf_worker_all_pools; wp; wp = wp->next) {
		size_t scoreboard_size, scoreboard_nprocs_size;
		void *shm_mem;
		int i;

		if (wp->config->pm_max_children < 1) {
			plog(PLOG_ERROR, "[pool %s] Unable to create scoreboard SHM because max_client is not set", wp->config->name);
			return -1;
		}

		if (wp->scoreboard) {
			plog(PLOG_ERROR, "[pool %s] Unable to create scoreboard SHM because it already exists", wp->config->name);
			return -1;
		}

		scoreboard_size        = sizeof(PMF_SCOREBOARD_S) + (wp->config->pm_max_children) * sizeof(PMF_SCOREBOARD_PROC_S *);
		scoreboard_nprocs_size = sizeof(PMF_SCOREBOARD_PROC_S) * wp->config->pm_max_children;
		shm_mem                = pmf_shm_alloc(scoreboard_size + scoreboard_nprocs_size);

		if (!shm_mem) {
			return -1;
		}
		wp->scoreboard         = shm_mem;
		wp->scoreboard->nprocs = wp->config->pm_max_children;
		shm_mem               += scoreboard_size;

		for (i = 0; i < wp->scoreboard->nprocs; i++, shm_mem += sizeof(PMF_SCOREBOARD_PROC_S)) {
			wp->scoreboard->procs[i] = shm_mem;
		}

		wp->scoreboard->pm          = wp->config->pm_type;
		wp->scoreboard->start_epoch = time(NULL);
		strncpy(wp->scoreboard->pool, wp->config->name, sizeof(wp->scoreboard->pool));
	}
}

