#include "pmf_events.h"

static PMF_EVENT_MODULE_S *pmf_event_module;

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

