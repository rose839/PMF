
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "pmf_log.h"

int pmf_stdio_init_main() {
	int fd = open("/dev/null", O_RDWR);

	if (fd < 0) {
		plog(PLOG_SYSERROR, "failed to init stdio: open(\"/dev/null\") failed!");
		return -1;
	}

	if (0 > dup2(fd, STDIN_FILENO) || 0 > dup2(fd, STDOUT_FILENO)) {
		zlog(ZLOG_SYSERROR, "failed to init stdio: dup2()");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

