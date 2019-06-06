
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "../pmf_event.h"
#include "../pmf_log.h"

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
	return 0;
}

static int pmf_event_select_wait(PMF_EVENT_QUEUE_S *queue, unsigned long int timeout) {
	int ret;
	PMF_EVENT_QUEUE_S *q;
	fd_set current_fds;
	struct timeval t;

	/* copy fds because select() alters it */
	current_fds = fds;

	t.tv_sec = timeout / 1000;
	t.tv_usec = (timeout % 1000) * 1000;
	ret = select(FD_SETSIZE, &current_fds, NULL, NULL, &t);
	if (-1 == ret) {
		if (errno != EINTR) {
			plog(PLOG_WARNING, "select() return %d", errno);
			return -1;
		}
	}

	if (ret > 0) {
		q = queue;
		while (q) {
			if (q->ev && FD_ISSET(q->ev->fd, &current_fds)) {
				pmf_event_proc(q->ev);
			}
			q = q->next;
		}
	}

	return ret;
}

static int pmf_event_select_add(PMF_EVENT_S *ev) {
	if (ev->fd >= FD_SETSIZE) {
		plog(PLOG_ERROR, "select: not enough space in the select fd list (max = %d). Please consider using another event mechanism.", FD_SETSIZE);
		return -1;
	}

	if (!FD_ISSET(ev->fd, &fds)) {
		FD_SET(ev->fd, &fds);
	}
	return 0;
}

static int pmf_event_select_remove(PMF_EVENT_S *ev) {
	if (FD_ISSET(ev->fd, &fds)) {
		FD_CLR(ev->fd, &fds);
	}

	return 0;
}

