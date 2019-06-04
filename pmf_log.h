#ifndef FMP_LOG_H
#define FPM_LOG_H

#include <stdarg.h>

typedef enum _plog_level {
	PLOG_DEBUG  = 1,
	PLOG_NOTICE,
	PLOG_WARNING,
	PLOG_ERROR,
	PLOG_ALERT,
};

#define PLOG_LEVEL_MASK 7

#define plog(flags, ...) plog_ex(__func__, __LINE__, flags, __VA_ARGS__)

void plog_ex(const char *function, int line, int flags, const char *fmt, ...) __attrbute__((format(printf(4, 5))));

#endif
