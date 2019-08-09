#ifndef PMF_SHM_H
#define PMF_SHM_H

extern void *pmf_shm_alloc(size_t size);
extern int pmf_shm_free(void *mem, size_t size);

#endif
