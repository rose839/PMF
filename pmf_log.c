#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <errno.h>

#include "pmf.h"
#include "pmf_log.h"

#define MAX_LINE_LENGTH 1024

static int plog_fd = -1;
static int plog_level = PLOG_NOTICE;
static int launched = 0;
static void (*external_logger)(int, char*, size_t) = NULL;

static const char *level_names[] = {
	[PLOG_DEBUG]   = "DEBUG",
	[PLOG_NOTICE]  = "NOTICE",
	[PLOG_WARNING] = "WARNING",
	[PLOG_ERROR]   = "ERROR",
	[PLOG_ALERT]   = "ALERT",
};

const char* plog_get_level_name(int log_level) {
	if (log_level < 0) {
		log_level = plog_level;
	} else if (log_level < PLOG_DEBUG || log_level > PLOG_ALERT) {
		return "unknown value";
	}

	return level_names[log_level];
}
size_t plog_print_time(struct timeval *tv, char *timebuf, size_t timebuf_len) {
	size_t len;

	len = strftime(timebuf, timebuf_len, "[%d-%b-%Y %H:%M:%S", localtime((const time_t*)&tv->tv_sec));
	if (plog_level == PLOG_DEBUG) {
		len += snprintf(timebuf + len, timebuf_len - len, ".%06d", (int) tv->tv_usec);
	}
	len += snprintf(timebuf + len, timebuf_len - len, "] ");

	return len;
}

int plog_set_fd(int new_fd) {
	int old_fd = plog_fd;

	plog_fd = new_fd;
	return old_fd;
}

int plog_set_level(int new_value) {
	int old_value = plog_level;

	if (new_value < PLOG_DEBUG || new_value > PLOG_ALERT) return old_value;

	plog_level = new_value;
	return old_value;
}

static vplog(const char *function, int line, int flags, const char *fmt, va_list args) {
	struct timeval tv;
	char buf[MAX_LINE_LENGTH];
	const size_t buf_size = MAX_LINE_LENGTH;
	size_t len = 0;
	int truncated = 0;
	int saved_errno;

	if (external_logger) {
		va_list ap;
		va_copy(ap, args);
		len = vsnprintf(buf, buf_size, fmt, ap);
		va_end(ap);
		if (len >= buf_size) {
			memcpy(buf + buf_size - sizeof("..."), "...", sizeof("...") - 1);
			len = buf_size - 1;
		}
		external_logger(flags & PLOG_LEVEL_MASK, buf, len);
		len = 0;
		memset(buf, '\0', buf_size);
	}

	if ((flags & PLOG_LEVEL_MASK) < plog_level) {
		return;
	}

	saved_errno = errno;
	if (!pmf_globals.is_child) {
		gettimeofday(&tv, 0);
		len = plog_print_time(&tv, buf, buf_size);
	}
	if (plog_level == PLOG_DEBUG) {
		if (!pmf_globals.is_child) {
			len += snprintf(buf+len, buf_size-len, "%s: pid %d, %s(), line %d: ", level_names[flags & PLOG_LEVEL_MASK], getpid(), function, line);
		} else {
			len += snprintf(buf + len, buf_size - len, "%s: %s(), line %d: ", level_names[flags & PLOG_LEVEL_MASK], function, line);
		}
	} else {
		len += snprintf(buf + len, buf_size - len, "%s: ", level_names[flags & PLOG_LEVEL_MASK]);
	}

	if (len > buf_size -1) {
		truncated = 1;
	}
	if (!truncated) {
		len += vsnprintf(buf + len, buf_size - len, fmt, args);
		if (len >= buf_size) {
			truncated = 1;
		}
	}

	if (!truncated) {
		if (flags & PLOG_HAVE_ERRNO) {
			len += snprintf(buf + len, buf_size - len, ": %s (%d)", strerror(saved_errno), saved_errno);
			if (len >= buf_size) {
				truncated = 1;
			}
		}
	}

	if (truncated) {
		memcpy(buf + buf_size - sizeof("..."), "...", sizeof("...") - 1);
		len = buf_size - 1;
	}

	buf[len++] = '\n';
	plog_quiet_write(plog_fd > -1 ? plog_fd : STDERR_FILENO, buf, len);

	if (plog_fd != STDERR_FILENO && plog_fd != -1 && !launched && (flags & PLOG_LEVEL_MASK) >= PLOG_NOTICE) {
		plog_quiet_write(STDERR_FILENO, buf, len);
	}
	
}

void plog_ex(const char *function, int line, int flags, const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vplog(function, line, flags, fmt, args);
	va_end(args);
}

