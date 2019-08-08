#ifndef PMF_SCOREBOARD_H
#define PMF_SCOREBOARD_H

typedef struct _pmf_scoreboard_proc_s {
	union {
		atomic_t lock;
		char dummy[16];
	};
	int used;
	time_t start_epoch;
	pid_t pid;
	unsigned long requests;
	struct timeval accepted;
	struct timeval duration;
	time_t accepted_epoch;
	struct timeval tv;
	size_t memory;
}PMF_SCOREBOARD_PROC_S;

typedef struct _pmf_scoreboard_s {
	union {
		atomic_t lock;
		char dummy[16];
	};
	char pool[32];
	int pm;
	time_t start_epoch;
	int idle;
	int active;
	int active_max;
	unsigned long int requests;
	unsigned int max_children_reached;
	int lq;              // socket listen queue current value(connet request num currently)
	int lq_max;          // socket listen queue max length(always geater or equal lq_len)
	unsigned int lq_len; // socket listen queue set length(if set value > system max value, then this is system max value)
	unsigned int nprocs;
	int free_proc;
	unsigned long int slow_rq;
	PMF_SCOREBOARD_PROC_S *procs[];
}PMF_SCOREBOARD_S;

#define PMF_SCOREBOARD_ACTION_SET 0
#define PMF_SCOREBOARD_ACTION_INC 1

extern int pmf_scoreboard_init_main();
void pmf_scoreboard_update(int idle, int active, int lq, int lq_len, int requests, int max_children_reached, int slow_rq, int action, PMF_SCOREBOARD_S *scoreboard);

#endif
