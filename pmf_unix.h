#ifndef PMF_UNIX_H
#define PMF_UNIX_H

#include "pmf_worker_pool.h"

extern int pmf_unix_set_socket_premissions(PMF_WORKER_POOL_S *wp, const char *path);
extern int pmf_unix_resolve_socket_premissions(PMF_WORKER_POOL_S *wp);
extern int pmf_unix_init_main();

#endif
