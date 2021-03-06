/* This header file contains structures that described configuration in conf file */

#ifndef PMF_CONF_H
#define PMF_CONF_H

#include <stddef.h>

/* global configuration */
typedef struct _pmf_global_config_s {
	char *pid_file;
	char *include_dir;
	char *error_log;
	int log_level;
	int emergency_restart_threshold;
	int emergency_restart_interval;
	int process_control_timeout;
	int process_max;
	int process_priority;
	int daemonize;
	int rlimit_files;
	int rlimit_core;
	char *event_mechanism;
}PMF_GLOBAL_CONFIG_S;

typedef enum _pm_type_e{
	PM_STYLE_STATIC = 1,
	PM_STYLE_DYNAMIC = 2,
	PM_STYLE_ONDEMAND = 3
}PM_TYPE_E;

/* worker pool configuration */
typedef struct _pmf_worker_pool_config_s {
	char *name;
	char *user;
	char *group;
	char *listen_address;

	/* for unix socket */
	char *listen_owner;
	char *listen_group;
	char *listen_mode;
	
	int listen_backlog;
	int process_priority;
	int process_dumpable;
	int pm_type;
	int pm_max_children;
	int pm_start_servers;
	int pm_min_spare_servers;
	int pm_max_spare_servers;
	int pm_process_idle_timeout;
	int pm_max_requests;
}PMF_WORKER_POOL_CONFIG_S;

/* this struct is used for parse key-value in ini file */
typedef struct _ini_value_parser_s {
	char *key;
	char*(*parser)(const char *value, void **config, int offset);
	int offset;
}INI_VALUE_PARSER_S;

#define PMF_CONF_FILE "/etc/pmf.conf"
#define MAX_KEY_VALUE_LEN  128UL
#define STR2STR(a) (a ? a : "undefined")
#define BOOL2STR(a) (a ? "yes" : "no")
#define PM2STR(a) (a == PM_STYLE_STATIC ? "static" : (a == PM_STYLE_DYNAMIC ? "dynamic" : "ondemand"))
#define GO(field) offsetof(PMF_GLOBAL_CONFIG_S, field)
#define WPO(field) offsetof(PMF_WORKER_POOL_CONFIG_S, field)

#ifndef MAX_PATH
#define MAX_PATH 128
#endif

extern PMF_GLOBAL_CONFIG_S pmf_global_config;

extern int pmf_conf_write_pid();
extern int pmf_conf_init_main();

#endif

