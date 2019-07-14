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

static int pmf_event_epoll_wait(PMF_EVENT_QUEUE_S *queue, unsigned long timeout) {
	int ret, i;

	memset(event_buf, 0, sizeof(struct epoll_event) * event_buf_num);

	ret = epoll_wait(epollfd, epoll_fd, event_buf, timeout);
	if (ret == -1) {

		if (errno != EINTR) {
			plog(PLOG_WARNING, "epoll_wait() returns %d", errno);
			return -1;
		}
	}

	for (i = 0; i < ret; i++) {

		if (!event_buf[i].data.ptr) {
			continue;
		}

		pmf_event_proc((PMF_EVENT_S *)event_buf[i].data.ptr);
	}

	return ret;
}

static int pmf_event_epol_add(PMF_EVENT_S *event) {
	struct epoll_event e;

	e.events = EPOLLIN;
	e.data.ptr = (void *)event;

	if (event->flags & PMF_EV_EDGE_TRIGGER) {
		e.events |= EPOLLET;
	}

	if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event->fd, &e)) {
		plog(PLOG_ERROR, "epoll: unable to add fd %d", event->fd);
		return -1;
	}

	return 0;
}

static int pmf_event_epol_remove(PMF_EVENT_S *event) {
	struct epoll_event e;

	e.events = EPOLLIN;
	e.data.ptr = (void *)event;

	if (event->flags & PMF_EV_EDGE_TRIGGER) {
		e.events |= EPOLLET;
	}

	if (-1 == epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event->fd, &e)) {
		plog(PLOG_ERROR, "epoll: unable to remove fd %d", event->fd);
		return -1;
	}

	return 0;
}

