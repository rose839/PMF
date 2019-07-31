
#ifndef PMF_EVENTS_H
#define PMF_EVENTS_H

/* event type */
#define PMF_EV_TIMEOUT (1 << 0)
#define PMF_EV_READ    (1 << 1)

/* event flag */
#define PMF_EV_LEVEL_TRIGGER (1 << 0)
#define PMF_EV_EDGE_TRIGGER  (1 << 1)

typedef struct _pmf_event_s {
	int fd;
	void (*callback)(struct _pmf_event_s *event, int event_type, void *arg);
	void *arg;
	int flags;
	int event_type;
}PMF_EVENT_S;

typedef struct _pmf_event_queue_s {
	struct _pmf_event_queue_s *prev;
	struct _pmf_event_queue_s *next;
	PMF_EVENT_S *ev;
} PMF_EVENT_QUEUE_S;

typedef struct _pmf_event_module_s {
	const char *name;
	int support_edge_trigger;
	int (*init)(int max_id);
	int (*clean)(void);
	int (*wait)(PMF_EVENT_QUEUE_S *queue, unsigned long timeout);
	int (*add)(PMF_EVENT_S *ev);
	int (*remove)(PMF_EVENT_S *ev);
}PMF_EVENT_MODULE_S;

extern int pmf_event_pre_init(const char *mechanism);
extern void pmf_event_proc(PMF_EVENT_S *ev);
extern int pmf_event_support_edge_trigger();

#endif