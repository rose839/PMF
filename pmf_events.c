#include "pmf_events.h"

static PMF_EVENT_MODULE_S *pmf_event_module;

void pmf_event_proc(PMF_EVENT_S *ev) {
	if (!ev || !ev->callback) {
		return;
	}

	(*ev->callback)(ev, ev->event_type, ev->arg)
}
