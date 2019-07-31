#include <time.h>
#include <sys/time.h>
#include "pmf_log.h"

static int monotonic_works;

int pmf_clock_init() {
	struct timespec ts;

	monotonic_works = 0;

	if (0 == clock_gettime(CLOCK_MONOTONIC, &ts)) {
		monotonic_works = 1;
	}

	return 0;
}

int pmf_clock_get(struct timeval *tv) {
	if (monotonic_works) {
		struct timespec ts;

		if (0 > clock_gettime(CLOCK_MONOTONIC, &ts)) {
			plog(PLOG_SYSERROR, "clock_gettime() failed");
			return -1;
		}

		tv->tv_sec = ts.tv_sec;
		tv->tv_usec = ts.tv_nsec / 1000;
		return 0;
	}

	return gettimeofday(tv, 0);
}

