
#include <sys/epoll.h>
#include <errno.h>

static int pmf_event_epoll_init(int max);
static int pmf_event_epoll_clean();
static int pmf_event_epoll_wait(PMF_EVENT_QUEUE_S *queue, unsigned long timeout);
static int pmf_event_epoll_add(PMF_EVENT_S *ev);
static int pmf_event_epoll_remove(PMF_EVENT_S *ev);

static PMF_EVENT_MODULE_S epoll_module = {
	.name = "epoll",
	.support_edge_trigger = 1,
	.init = pmf_event_epoll_init,
	.clean = pmf_event_epoll_clean,
	.wait = pmf_event_epoll_wait,
	.add = pmf_event_epoll_add,
	.remove = pmf_event_epoll_remove,
};

static struct epoll_event *event_buf = NULL;
static int event_buf_num;
static int epoll_fd = -1;

PMF_EVENT_MODULE_S *pmf_event_epoll_module() {
	return &epoll_module;
}

static int pmf_event_epoll_init(int max) {
	if (max < 1) {
		return 0;
	}

	epoll_fd = epoll_create(max+1);
	if (epoll_fd < 0) {
		plog(PLOG_ERROR, "epoll: unable to initialize.");
		return -1;
	}

	event_buf = malloc(sizeof(struct epoll_event) * max);
	if (!event_buf) {
		plog(PLOG_ERROR, "epoll: unable to allocate %d events", max);
		return -1;
	}
	memset(event_buf, 0, sizeof(struct epoll_event) * max);

	event_buf_num = max;

	return 0;
}

static int pmf_event_epoll_clean() {
	if (event_buf) {
		free(event_buf);
		event_buf = NULL;
		event_buf_num = -1;
	}
	if (-1 != epoll_fd) {
		close(epoll_fd);
		epoll_fd = -1;
	}

	return -1;
}
