
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "../pmf_event.h"

static int pmf_event_select_init(int max);
static int pmf_event_select_wait(PMF_EVENT_QUEUE_S *queue, unsigned long int timeout);
static int pmf_event_select_add(PMF_EVENT_S *ev);
static int pmf_event_select_remove(PMF_EVENT_S *ev);

static PMF_EVENT_MODULE_S select_module = {
	.name = "select",
	.support_edge_trigger = 0,
	.init = pmf_event_select_init,
	.clean = NULL,
	.wait = pmf_event_select_wait,
	.add = pmf_event_select_add,
	.remove = pmf_event_select_remove,
};

static fs_set fds;

PMF_EVENT_MODULE_S *pmf_event_select_module() {
	return &select_module;
	return 0;
}

static int pmf_events_select_init(int max) {
	FD_ZERO(&fds);
}

static int fpm_event_select_wait(struct fpm_event_queue_s *queue, unsigned long int timeout) {

}

static int fpm_event_select_add(struct fpm_event_s *ev) {

}

static int fpm_event_select_remove(struct fpm_event_s *ev) {

}

