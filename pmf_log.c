
#include <stdarg.h>
#include "pmf_log.h"

static int plog_fd = -1;
static int plog_level = PLOG_NOTICE;
static int launched = 0;
static void (*external_logger)(int, char*, size_t) = NULL;

static const char *level_name[] = {
	[PLOG_DEBUG]   = "DEBUG",
	[PLOG_NOTICE]  = "NOTICE",
	[PLOG_WARNING] = "WARNING",
	[PLOG_ERROR]   = "ERROR",
	[PLOG_ALERT]   = "ALERT",
};

static vplog(const char *function, int line, int flags, va_list args) {
	
}

void plog_ex(const char *function, int line, int flags, const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vplog(function, line, flags, fmt, args);
	va_end(args);
}

