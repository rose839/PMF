#include "pmf_events.h"
#include "pmf_log.h"
#include "epoll.h"
#include "select.h"

static PMF_EVENT_MODULE_S *pmf_event_module;

int pmf_event_support_edge_trigger() {
	return pmf_event_module ? pmf_event_module->support_edge_trigger : 0;
}

void pmf_event_proc(PMF_EVENT_S *ev) {
	if (!ev || !ev->callback) {
		return;
	}

	(*ev->callback)(ev, ev->event_type, ev->arg);
}

int pmf_event_pre_init(const char *mechanism) {
	/* epoll */
	pmf_event_module = pmf_event_epoll_module();
	if (pmf_event_module) {
		if (!mechanism || strcasecmp(pmf_event_module->name, mechanism) == 0) {
			return 0;
		}
	}

	/* select */
	pmf_event_module = pmf_event_select_module();
	if (pmf_event_module) {
		if (!mechanism || strcasecmp(pmf_event_module->name, mechanism) == 0) {
			return 0;
		}
	}

	if (mechanism) {
		plog(PLOG_ERROR, "event mechanism '%s' is not available on this system", mechanism);
	} else {
		plog(PLOG_ERROR, "unable to find a suitable event mechanism on this system");
	}
	return -1;
}

int pmf_event_init_main() {
	PMF_WORKER_POOL_S *wp;
	int max;

	if (!pmf_event_module) {
		plog(PLOG_ERROR, "no event module found");
		return -1;
	}

	if (!pmf_event_module->wait) {
		plog(PLOG_ERROR, "Incomplete event implementation. ");
		return -1;
	}

	/* count the max number of necessary fds for polling */
	max = 1; /* only one FD is necessary at startup for the master process signal pipe */
	for (wp = pmf_worker_all_pools; wp; wp = wp->next) {
		if (!wp->config) continue;
		if (wp->config->pm_max_children > 0) {
			max += (wp->config->pm_max_children * 2);
		}
	}

	if (pmf_event_module->init(max) < 0) {
		plog(PLOG_ERROR, "Unable to initialize the event module %s", module->name);
		return -1;
	}

	plog(PLOG_DEBUG, "event module is %s and %d fds have been reserved", module->name, max);

	return 0;
}


