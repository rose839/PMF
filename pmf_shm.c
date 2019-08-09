#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "pmf_shm.h"
#include "pmf_log.h"

static size_t pmf_shm_size = 0;

void *pmf_shm_alloc(size_t size) {
	void *mem;

	mem = mmap(0, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);

	if (NULL == mem) {
		plog(PLOG_SYSERROR, "unable to allocate %zu bytes in shared memory", size);
		return NULL;
	}

	pmf_shm_size += size;

	return mem;
}

int pmf_shm_free(void *mem, size_t size) {
	if (!mem) {
		plog(PLOG_ERROR, "mem is NULL");
		return 0;
	}

	if (munmap(mem, size) == -1) {
		plog(PLOG_SYSERROR, "Unable to free shm");
		return 0;
	}

	if (pmf_shm_size - size > 0) {
		pmf_shm_size -= size;
	} else {
		pmf_shm_size = 0;
	}

	return 1;
}


