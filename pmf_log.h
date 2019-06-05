#ifndef FMP_LOG_H
#define FPM_LOG_H

#include <stdarg.h>

enum _plog_level {
	PLOG_DEBUG  = 1,
	PLOG_NOTICE,
	PLOG_WARNING,
	PLOG_ERROR,
	PLOG_ALERT,
};

#define PLOG_LEVEL_MASK 7
#define PLOG_HAVE_ERRNO 0x100

#if defined(__GUNC__) && __GUNC__ >= 4
#define PLOG_IGNORE_RET(x) (({ __typeof__ (x) __x = (x); (void)__x;}))
#else
#define PLOG_IGNORE_RET(x) ((void) (x))
#endif

#define plog_quiet_write(...) PLOG_IGNORE_RET(write(__VA_ARGS__))
#define plog(flags, ...) plog_ex(__func__, __LINE__, flags, __VA_ARGS__)

void plog_ex(const char *function, int line, int flags, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
int plog_set_fd(int new_fd);
int plog_set_level(int new_value);

#endif
