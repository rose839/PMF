/* This header file contains structures that described configuration in conf file */

#ifndef PMF_CONF_H
#define PMF_CONF_H

/* global configuration */
struct pmf_global_config_s {
	char *pid_file;
	char *error_log;
	int log_level;
	int emergency_restart_threshold;
	int emergency_restart_interval;
	int process_control_timeout;
	int process_max;
	int process_priority;
	int deamonize;
	int rlimit_files;
	int rlimit_core;
	char *event_mechanism;
};

/* worker pool configuration */
struct pmf_worker_pool_config_s {
	char *name;
	char *prefix;
	char *user;
	char *group;
	char *listen_address;
	int process_priority;
	int pm;
	
};

#endif

