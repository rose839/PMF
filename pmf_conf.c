#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "pmf.h"
#include "pmf_conf.h"
#include "pmf_log.h"
#include "pmf_events.h"
#include "pmf_sockets.h"
#include "pmf_worker_pool.h"
#include "iniparser.h"

static char *pmf_config_set_string(const char *value, void **config, int offset);
static char *pmf_conf_set_log_level(const char *value, void **config, int offset);
static char *pmf_conf_set_integer(const char *value, void **config, int offset);
static char *pmf_conf_set_time(const char *value, void **config, int offset);
static char *fpm_conf_set_boolean(const char *value, void **config, int offset);
static char *pmf_conf_set_rlimit_core(const char *value, void **config, int offset);
static char *pmf_conf_set_pm(const char *value, void **config, int offset);

static INI_VALUE_PARSER_S pmf_ini_global_option[] = {
	{"pid_file",                    pmf_config_set_string,      GO(pid_file)},
	{"include_dir",                 pmf_config_set_string,      GO(include_dir)},
	{"error_log",                   pmf_config_set_string,      GO(error_log)},
	{"log_level",                   pmf_conf_set_log_level,     GO(log_level)},
	{"emergency_restart_threshold", pmf_conf_set_integer,       GO(emergency_restart_threshold)},
	{"emergency_restart_interval",  pmf_conf_set_time,          GO(emergency_restart_interval)},
	{"process_control_timeout",     pmf_conf_set_time,          GO(process_control_timeout)},
	{"process.max",                 pmf_conf_set_integer,       GO(process_max)},
	{"process.priority",            pmf_conf_set_integer,       GO(process_priority)},
	{"daemonize",                   fpm_conf_set_boolean,       GO(daemonize)},
	{"rlimit_files",                pmf_conf_set_integer,       GO(rlimit_files)},
	{"rlimit_core",                 pmf_conf_set_rlimit_core,   GO(rlimit_core)},
	{"events.mechanism",            pmf_config_set_string,      GO(event_mechanism)},
};

static INI_VALUE_PARSER_S pmf_ini_pool_option[] = {
	{"user",                        pmf_config_set_string,    WPO(user)},
	{"group",                       pmf_config_set_string,    WPO(group)},
	{"listen",                      pmf_config_set_string,    WPO(listen_address)},
	{"listen_backlog",              pmf_conf_set_integer,     WPO(listen_backlog)},
	{"process_priority",            pmf_conf_set_integer,     WPO(process_priority)},
	{"process.dumpable",            pmf_conf_set_integer,     WPO(process_dumpable)},
	{"pm",                          pmf_conf_set_pm,          WPO(pm_type)},
	{"pm.max_children",             pmf_conf_set_integer,     WPO(pm_max_children)},
	{"pm.start_servers",            pmf_conf_set_integer,     WPO(pm_start_servers)},
	{"pm.min_spare_servers",        pmf_conf_set_integer,     WPO(pm_min_spare_servers)},
	{"pm.max_spare_servers",        pmf_conf_set_integer,     WPO(pm_max_spare_servers)},
	{"pm.process_idle_timeout",     pmf_conf_set_integer,     WPO(pm_process_idle_timeout)},
	{"pm.max_requests",             pmf_conf_set_integer,     WPO(pm_max_requests)},
};

PMF_GLOBAL_CONFIG_S pmf_global_config = {
	.log_level = PLOG_ALERT,
	.daemonize = 0,
	.process_max = 0,
	.process_priority = 64, /* 64 means unset */
};

static PMF_WORKER_POOL_S *current_wp = NULL;

/*
 * Expand the '$pool' token in a dynamically allocated string
 */
static int pmf_conf_expand_pool_name(char **value) {
	char *token;

	if (!value || !*value) {
		return 0;
	}

	while (*value && (token = strstr(*value, "$pool"))) {
		char buf[MAX_KEY_VALUE_LEN];
		char *p2 = token + strlen("$pool");

		/* If we are not in a pool, we cannot expand this name now */
		/*
		if (!current_wp || !current_wp->config  || !current_wp->config->name) {
			return -1;
		}
		*/

		/* "aaa$poolbbb" becomes "aaa\0oolbbb" */
		token[0] = '\0';

		/* Build a brand new string with the expanded token */
		snprintf(buf, MAX_KEY_VALUE_LEN, "%s%s%s", *value, /*current_wp->config->name*/ "pool", p2);

		/* Free the previous value and save the new one */
		free(*value);
		*value = strdup(buf);
	}

	return 0;
}

static char *pmf_config_set_string(const char *value, void **config, int offset) {
	char **config_val = (char**)((char*)*config + offset);

	if (NULL == config) {
		return "internal error: NULL value!";
	}

	/* free previous value */
	if (*config_val) {
		free(*config_val);
	}

	*config_val = strdup(value);
	if (NULL == *config_val) {
		return "pmf_conf_set_string(): strdup() failed!";
	}

	if (pmf_conf_expand_pool_name(config_val) == -1) {
		return "Can't use '$pool' when the pool is not defined";
	}

	return NULL;
}

static char *pmf_conf_set_log_level(const char *value, void **config, int offset) {
	int log_level;

	if (!strcasecmp(value, "debug")) {
		log_level = PLOG_DEBUG;
	} else if (!strcasecmp(value, "notice")) {
		log_level = PLOG_NOTICE;
	} else if (!strcasecmp(value, "warning")) {
		log_level = PLOG_WARNING;
	} else if (!strcasecmp(value, "error")) {
		log_level = PLOG_ERROR;
	} else if (!strcasecmp(value, "alert")) {
		log_level = PLOG_ALERT;
	} else {
		return "invalid value for 'log_level'";
	}

	* (int *) ((char *) *config + offset) = log_level;
	return NULL;
}

static char *pmf_conf_set_integer(const char *value, void **config, int offset) {
	const char *p;
	
	for (p = value; *p; p++) {
		if (*p < '0' || *p > '9') {
			return "is not a valid number (greater or equal than zero)";
		}
	}
	* (int *) ((char *) *config + offset) = atoi(value);
	return NULL;
}

static char *pmf_conf_set_rlimit_core(const char *value, void **config, int offset) {
	int *ptr = (int *) ((char *) *config + offset);

	if (!strcasecmp(value, "unlimited")) {
		*ptr = -1;
	} else {
		int int_value;
		void *subconf = &int_value;
		char *error;

		error = pmf_conf_set_integer(value, &subconf, 0);

		if (error) {
			return error;
		}

		if (int_value < 0) {
			return "must be greater than zero or 'unlimited'";
		}

		*ptr = int_value;
	}

	return NULL;
}

static char *pmf_conf_set_time(const char *value, void **config, int offset) {
	char *val = strdup(value);
	int len = strlen(val);
	char suffix;
	int seconds;
	if (!len) {
		return "invalid time value";
	}
	if (NULL == val) {
		return "strdup value failed";
	}

	suffix = val[len-1];
	switch (suffix) {
		case 'm' :
			val[len-1] = '\0';
			seconds = 60 * atoi(val);
			break;
		case 'h' :
			val[len-1] = '\0';
			seconds = 60 * 60 * atoi(val);
			break;
		case 'd' :
			val[len-1] = '\0';
			seconds = 24 * 60 * 60 * atoi(val);
			break;
		case 's' : /* s is the default suffix */
			val[len-1] = '\0';
			suffix = '0';
		default :
			if (suffix < '0' || suffix > '9') {
				return "unknown suffix used in time value";
			}
			seconds = atoi(val);
			break;
	}

	* (int *) ((char *) *config + offset) = seconds;
	free(val);
	
	return NULL;
}

static char *fpm_conf_set_boolean(const char *value, void **config, int offset) {
	long value_y = !strcasecmp(value, "yes");
	long value_n = !strcasecmp(value, "no");

	if (!value_y && !value_n) {
		return "invalid boolean value";
	}

	* (int *) ((char *) *config + offset) = value_y ? 1 : 0;
	return NULL;
}

static char *pmf_conf_set_pm(const char *value, void **config, int offset) {
	int *ptr = (int *) ((char *) *config + offset);

	if (!strcasecmp(value, "static")) {
		*ptr = PM_STYLE_STATIC;
	} else if (!strcasecmp(value, "dynamic")) {
		*ptr = PM_STYLE_DYNAMIC;
	} else if (!strcasecmp(value, "ondemand")) {
		*ptr = PM_STYLE_ONDEMAND;
	} else {
		return "invalid process manager (static, dynamic or ondemand)";
	}
	return NULL;
}

static int pmf_conf_is_dir(char *path) {
	struct stat sb;

	if (stat(path, &sb) != 0) {
		return 0;
	}

	return (sb.st_mode & S_IFMT) == S_IFDIR;
}

static void *pmf_worker_pool_config_alloc() {
	PMF_WORKER_POOL_S *wp;

	wp = pmf_worker_pool_alloc();

	if (NULL != wp) {
		return NULL;
	}

	wp->config = malloc(sizeof(PMF_WORKER_POOL_CONFIG_S));
	if (NULL == wp->config) {
		pmf_worker_pool_free(wp);
		return 0;
	}

	memset(wp->config, 0, sizeof(PMF_WORKER_POOL_CONFIG_S));
	wp->config->listen_backlog = PMF_BACKLOG_DEFAULT;
	wp->config->pm_process_idle_timeout = 10; /* 10s by default */
	wp->config->process_priority = 64;        /* 64 means unset */
	wp->config->process_dumpable = 0;

	if (NULL == pmf_worker_all_pools) {
		pmf_worker_all_pools = wp;
	} else {
		PMF_WORKER_POOL_S *tmp = pmf_worker_all_pools;
		while (tmp) {
			if (NULL == tmp->next) {
				tmp->next = wp;
				break;
			}
			tmp = tmp->next;
		}
	}

	current_wp = wp;
	return wp->config;
}

static int pmf_conf_parse_pool_ini(dictionary *ini) {
	void *config;
	int section_count;
	int index;
	const char *value;
	char real_key[MAX_KEY_VALUE_LEN];
	const char *ret;
	PMF_WORKER_POOL_S *wp;
	PMF_WORKER_POOL_CONFIG_S *wp_config;

	section_count = iniparser_getnsec(ini);
	if (-1 == section_count) {
		plog(PLOG_ERROR, "worker pool ini file get section count failed!");
		return -1;
	}
	while (section_count > 0) {
		const char *section_name = iniparser_getsecname(ini, section_count-1);

		if (0 == strcmp(section_name, "global")) {
			plog(PLOG_ERROR, "cann't use name 'global' for work pool name!");
			return -1;
		}

		for (wp = pmf_worker_all_pools; wp; wp = wp->next) {
			if (!wp->config) continue;
			if (!wp->config->name) continue;
			if (!strcasecmp(wp->config->name, section_name)) {
				/* found a wp with the same name */
				plog(PLOG_ERROR, "cann't set same worker pool name '%s'!", section_name);
				return -1;
			}
		}

		/* a new wp */
		wp_config = pmf_worker_pool_config_alloc();
		if (!current_wp || !wp_config) {
			plog(PLOG_ERROR, "Unable to alloc a new WorkerPool for worker '%s'", section_name);
			return -1;
		}
		wp_config->name = strdup(section_name);
		if (!wp_config->name) {
			plog(PLOG_ERROR, "Unable to alloc memory for configuration name for worker '%s'", section_name);
			return -1;
		}

		/* get pool option */
		config = wp_config;
		for (index = 0; index < sizeof(pmf_ini_pool_option)/sizeof(INI_VALUE_PARSER_S); index++) {
			real_key[0] = '\0';

			snprintf(real_key, MAX_KEY_VALUE_LEN, "%s:%s", section_name, pmf_ini_pool_option[index].key);
			value = iniparser_getstring(ini, real_key, NULL);
			if (NULL != value) {
				ret = pmf_ini_pool_option[index].parser(value, &config, pmf_ini_pool_option[index].offset);
				if (NULL != ret) {
					plog(PLOG_ERROR, "section '%s' option '%s' parse failed!", pmf_ini_pool_option[index].key);
					return -1;
				}
			} else {
				plog(PLOG_NOTICE, "section '%s' option '%s' doesn't config.", pmf_ini_pool_option[index].key);
			}
		}
		
		section_count--;
	}
}

static int pmf_conf_parse_pool_config(const char *include_dir) {
	DIR *d = NULL;
    struct dirent *dp = NULL;
    struct stat st;    
    char path[MAX_PATH] = {0};
	dictionary *ini;

    if(!(d = opendir(include_dir))) {
        plog(PLOG_ERROR, "open config file '%s' failed!", include_dir);
        return -1;
    }

    while((dp = readdir(d)) != NULL) {
        if((!strncmp(dp->d_name, ".", 1)) || (!strncmp(dp->d_name, "..", 2)))
            continue;

        snprintf(path, MAX_PATH, "%s/%s", include_dir, dp->d_name);
        if(0 < stat(path, &st) && !S_ISREG(st.st_mode)) {
             plog(PLOG_NOTICE, "there is not a config file '%s' in include dir.", dp->d_name);
			 continue;
        }

		ini = iniparser_load(path);
		if (NULL == ini) {
			plog(PLOG_ERROR, "iniparser load worker pool ini file '%s' failed!", path);
			return -1;
		}

		/* parse single pool option */
		if (0 > pmf_conf_parse_pool_ini(ini)) {
			plog(PLOG_ERROR, "iniparser load worker pool ini file '%s' failed!", path);
			return -1;
		}
    }
    closedir(d);

	return 0;
}

static int pmf_conf_parse_global_ini(dictionary *ini) {
	int index;
	const char *value;
	int section_count;
	char real_key[MAX_KEY_VALUE_LEN];
	char include_dir[MAX_KEY_VALUE_LEN];
	void *config;
	char *ret;

	/* get global option */
	config = &pmf_global_config;
	for (index = 0; index < sizeof(pmf_ini_global_option)/sizeof(INI_VALUE_PARSER_S); index++) {
		real_key[0] = '\0';

		snprintf(real_key, MAX_KEY_VALUE_LEN, "global:%s", pmf_ini_global_option[index].key);
		value = iniparser_getstring(ini, real_key, NULL);
		if (NULL != value) {
			ret = pmf_ini_global_option[index].parser(value, &config, pmf_ini_global_option[index].offset);
			if (NULL != ret) {
				plog(PLOG_ERROR, "global option '%s' parse failed: %s.", pmf_ini_global_option[index].key, ret);
				return -1;
			}
		} else {
			plog(PLOG_NOTICE, "global option '%s' doesn't config.", pmf_ini_global_option[index].key);
		}
	}

	/* get pool option */
	config = &pmf_global_config;
	section_count = iniparser_getnsec(ini);
	if (-1 == section_count) {
		plog(PLOG_ERROR, "ini file get section count failed!");
		return -1;
	}
	while (section_count > 0) {
		const char *section_name = iniparser_getsecname(ini, section_count-1);

		if (0 == strcmp(section_name, "global")) {
			section_count--;
			continue;
		}
		/* get pool option */
		for (index = 0; index < sizeof(pmf_ini_pool_option)/sizeof(INI_VALUE_PARSER_S); index++) {
			real_key[0] = '\0';

			snprintf(real_key, MAX_KEY_VALUE_LEN, "%s:%s", section_name, pmf_ini_global_option[index].key);
			value = iniparser_getstring(ini, real_key, NULL);
			if (NULL != value) {
				ret = pmf_ini_pool_option[index].parser(value, &config, pmf_ini_pool_option[index].offset);
				if (NULL != ret) {
					plog(PLOG_ERROR, "section '%s' option '%s' parse failed!", pmf_ini_pool_option[index].key);
					return -1;
				}
			} else {
				plog(PLOG_NOTICE, "section '%s' option '%s' doesn't config.", pmf_ini_pool_option[index].key);
			}
		}
		
		section_count--;
	}

	return 0;
}

static void pmf_conf_dump() /* {{{ */
{
	PMF_WORKER_POOL_S *wp;

	plog(PLOG_NOTICE, "[global]");
	plog(PLOG_NOTICE, "\t\tpid = %s",                         STR2STR(pmf_global_config.pid_file));
	plog(PLOG_NOTICE, "\t\tinclude_dir = %s",                 STR2STR(pmf_global_config.include_dir));
	plog(PLOG_NOTICE, "\t\terror_log = %s",                   STR2STR(pmf_global_config.error_log));
	plog(PLOG_NOTICE, "\t\tlog_level = %s",                   plog_get_level_name(pmf_global_config.log_level));
	plog(PLOG_NOTICE, "\t\temergency_restart_interval = %ds", pmf_global_config.emergency_restart_interval);
	plog(PLOG_NOTICE, "\t\temergency_restart_threshold = %d", pmf_global_config.emergency_restart_threshold);
	plog(PLOG_NOTICE, "\t\tprocess_control_timeout = %ds",    pmf_global_config.process_control_timeout);
	plog(PLOG_NOTICE, "\t\tprocess.max = %d",                 pmf_global_config.process_max);
	if (pmf_global_config.process_priority == 64) {
		plog(PLOG_NOTICE, "\t\tprocess.priority = undefined");
	} else {
		plog(PLOG_NOTICE, "\t\tprocess.priority = %d", pmf_global_config.process_priority);
	}
	plog(PLOG_NOTICE, "\t\tdaemonize = %s",                   BOOL2STR(pmf_global_config.daemonize));
	plog(PLOG_NOTICE, "\t\trlimit_files = %d",                pmf_global_config.rlimit_files);
	plog(PLOG_NOTICE, "\t\trlimit_core = %d",                 pmf_global_config.rlimit_core);
	plog(PLOG_NOTICE, "\t\tevents.mechanism = %s",            STR2STR(pmf_global_config.event_mechanism));

	for (wp = pmf_worker_all_pools; wp; wp = wp->next) {
		if (!wp->config) continue;
		plog(PLOG_NOTICE, "[%s]",                              STR2STR(wp->config->name));
		plog(PLOG_NOTICE, "\tuser = %s",                       STR2STR(wp->config->user));
		plog(PLOG_NOTICE, "\tgroup = %s",                      STR2STR(wp->config->group));
		plog(PLOG_NOTICE, "\tlisten = %s",                     STR2STR(wp->config->listen_address));
		plog(PLOG_NOTICE, "\tlisten.backlog = %d",             wp->config->listen_backlog);
		if (wp->config->process_priority == 64) {
			plog(PLOG_NOTICE, "\tprocess.priority = undefined");
		} else {
			plog(PLOG_NOTICE, "\tprocess.priority = %d", wp->config->process_priority);
		}
		plog(PLOG_NOTICE, "\tprocess.dumpable = %s",           BOOL2STR(wp->config->process_dumpable));
		plog(PLOG_NOTICE, "\tpm = %s",                         PM2STR(wp->config->pm_type));
		plog(PLOG_NOTICE, "\tpm.max_children = %d",            wp->config->pm_max_children);
		plog(PLOG_NOTICE, "\tpm.start_servers = %d",           wp->config->pm_start_servers);
		plog(PLOG_NOTICE, "\tpm.min_spare_servers = %d",       wp->config->pm_min_spare_servers);
		plog(PLOG_NOTICE, "\tpm.max_spare_servers = %d",       wp->config->pm_max_spare_servers);
		plog(PLOG_NOTICE, "\tpm.process_idle_timeout = %d",    wp->config->pm_process_idle_timeout);
		plog(PLOG_NOTICE, "\tpm.max_requests = %d",            wp->config->pm_max_requests);
	}
}

static int pmf_conf_load_conf_file() {
	dictionary *ini;
	int ret = 0;

	ini = iniparser_load(PMF_CONF_FILE);
	if (NULL == ini) {
		plog(PLOG_ERROR, "iniparser load ini file failed!");
		return -1;
	}

	if (0 > pmf_conf_parse_global_ini(ini)) {
		plog(PLOG_ERROR, "parse ini file failed!");
		return -1;
	}
	
	return ret;
}

int pmf_conf_write_pid() {
	int fd;

	if (pmf_global_config.pid_file) {
		char buf[64];
		int len;

		unlink(pmf_global_config.pid_file);
		fd = creat(pmf_global_config.pid_file, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		if (fd < 0) {
			plog(PLOG_SYSERROR, "Unable to create the PID file (%s).", pmf_global_config.pid_file);
			return -1;
		}

		len = sprintf(buf, "%d", (int) pmf_globals.parent_pid);

		if (len != write(fd, buf, len)) {
			plog(PLOG_SYSERROR, "Unable to write to the PID file.");
			close(fd);
			return -1;
		}
		close(fd);
	}
	return 0;
}

int pmf_conf_unlink_pid() {
	if (pmf_global_config.pid_file) {
		if (0 > unlink(pmf_global_config.pid_file)) {
			plog(PLOG_SYSERROR, "Unable to remove the PID file (%s).", pmf_global_config.pid_file);
			return -1;
		}
	}
	return 0;
}

static int pmf_conf_proc_config() {
	pmf_globals.log_level = pmf_global_config.log_level;
	plog_set_level(pmf_globals.log_level);

	if (NULL != pmf_global_config.include_dir &&
		pmf_conf_is_dir(pmf_global_config.include_dir) &&
		0 > pmf_conf_parse_pool_config(pmf_global_config.include_dir)) {
		plog(PLOG_ERROR, "parse worker pool config in include_dir '%s' failed!", pmf_global_config.include_dir);
		return -1;
	}

	if (pmf_global_config.process_max < 0) {
		plog(PLOG_ERROR, "process_max can't be negative");
		return -1;
	}

	if (pmf_global_config.process_priority != 64 && 
		(pmf_global_config.process_priority < -19 || pmf_global_config.process_priority > 20)) {
		plog(PLOG_ERROR, "process.priority must be included into [-19,20]");
		return -1;
	}

	if (NULL == pmf_global_config.error_log) {
		pmf_global_config.error_log = strdup(PMF_ERROR_LOG_FILE_DEFAULT);
	}

	if (0 > pmf_stdio_open_error_log(0)) {
		return -1;
	}

	if (0 > pmf_event_pre_init(pmf_global_config.event_mechanism)) {
		return -1;
	}

	return -1;
}

int pmf_conf_init_main() {
	int ret;

	ret = pmf_conf_load_conf_file(PMF_CONF_FILE);
	if (0 > ret) {
		plog(PLOG_ERROR, "failed to load configuration file '%s'!", PMF_CONF_FILE);
		return -1;
	}

	if (0 > pmf_conf_proc_config()) {
		plog(PLOG_ERROR, "failed to process the configuration!");
		return -1;
	}

	pmf_conf_dump();
	return ret;
}
