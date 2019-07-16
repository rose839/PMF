#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "pmf.h"
#include "pmf_conf.h"
#include "pmf_log.h"

static int pmf_use_error_log() {
	/*
	 * the error_log is NOT used when running in foreground
	 * and from a tty (user looking at output).
	 */
	if (pmf_global_config.daemonize || !isatty(STDERR_FILENO)) {
		return 1;
	}
	return 0;
}

int pmf_stdio_open_error_log(int reopen) {
	int fd;

	fd = open(pmf_global_config.error_log, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
	if (0 > fd) {
		plog(PLOG_SYSERROR, "failed to open error_log (%s)", pmf_global_config.error_log);
		return -1;
	}

	if (reopen) {
		if (pmf_use_error_log()) {
			dup2(fd, STDERR_FILENO);
		}

		dup2(fd, pmf_globals.error_log_fd);
		close(fd);
		fd = pmf_globals.error_log_fd;
	} else {
		pmf_globals.error_log_fd = fd;
		if (pmf_use_error_log()) {
			plog_set_fd(pmf_globals.error_log_fd);
		}
	}
	if (0 > fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC)) {
		plog(PLOG_WARNING, "failed to change attribute of error_log");
	}
	return 0;
}

int pmf_stdio_init_main() {
	int fd = open("/dev/null", O_RDWR);

	if (fd < 0) {
		plog(PLOG_SYSERROR, "failed to init stdio: open(\"/dev/null\") failed!");
		return -1;
	}

	if (0 > dup2(fd, STDIN_FILENO) || 0 > dup2(fd, STDOUT_FILENO)) {
		plog(PLOG_SYSERROR, "failed to init stdio: dup2()");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

