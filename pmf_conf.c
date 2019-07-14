
#include "pmf_log.h"
#include "pmf_events.h"
#include "plog.h"
#include "iniparser.h"

static INI_VALUE_PARSER_S pmf_ini_global_option[] = {
	{"pid_file",                    pmf_config_set_string,  GO(pid_file)},
	{"error_log",                   pmf_config_set_string,  GO(error_log)},
	{"log_level",                   pmf_conf_set_log_level, GO(log_level)},
	{"emergency_restart_threshold", pmf_conf_set_integer,   GO(emergency_restart_threshold)},
	{"emergency_restart_interval",  pmf_conf_set_time,      GO(emergency_restart_interval)},
	{"process_control_timeout",     pmf_conf_set_time,      GO(process_control_timeout)},
	{"process.max",                 pmf_conf_set_integer,   GO(process_max)},
	{"process.priority",            pmf_conf_set_integer,   GO(process_priority)},
	{"daemonize",                   fpm_conf_set_boolean,   GO(daemonize)},
	{"rlimit_files",                pmf_conf_set_integer,   GO(rlimit_files)},
	{"rlimit_core",                 pmf_conf_set_integer,   GO(rlimit_core)},
	{"events.mechanism",            pmf_conf_set_integer,   GO(event_mechanism)},
};

static INI_VALUE_PARSER_S pmf_ini_pool_option[] = {
	
};

PMF_GLOBAL_CONFIG_S pmf_global_config = {
	.daemonize = 0,
	.process_max = 0,
	.process_priority = 64, /* 64 means unset */
};

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
		if (!current_wp || !current_wp->config  || !current_wp->config->name) {
			return -1;
		}

		/* "aaa$poolbbb" becomes "aaa\0oolbbb" */
		token[0] = '\0';

		/* Build a brand new string with the expanded token */
		vsnprintf(buf, MAX_KEY_VALUE_LEN, "%s%s%s", *value, current_wp->config->name, p2);

		/* Free the previous value and save the new one */
		free(*value);
		*value = strdup(buf);
	}

	return 0;
}

static char *pmf_config_set_string(char *value, void **config, int offset) {
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

static char *pmf_conf_set_log_level(char *value, void **config, int offset)
{
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

static char *pmf_conf_set_integer(char *value, void **config, int offset)
{
	char *p;
	
	for (p = value; *p; p++) {
		if (*p < '0' || *p > '9') {
			return "is not a valid number (greater or equal than zero)";
		}
	}
	* (int *) ((char *) *config + offset) = atoi(value);
	return NULL;
}

static char *pmf_conf_set_time(char *value, void **config, int offset)
{
	int len = strlen(value);
	char suffix;
	int seconds;
	if (!len) {
		return "invalid time value";
	}

	suffix = value[len-1];
	switch (suffix) {
		case 'm' :
			value[len-1] = '\0';
			seconds = 60 * atoi(value);
			break;
		case 'h' :
			value[len-1] = '\0';
			seconds = 60 * 60 * atoi(value);
			break;
		case 'd' :
			value[len-1] = '\0';
			seconds = 24 * 60 * 60 * atoi(value);
			break;
		case 's' : /* s is the default suffix */
			value[len-1] = '\0';
			suffix = '0';
		default :
			if (suffix < '0' || suffix > '9') {
				return "unknown suffix used in time value";
			}
			seconds = atoi(value);
			break;
	}

	* (int *) ((char *) *config + offset) = seconds;
	return NULL;
}

static char *fpm_conf_set_boolean(char *value, void **config, int offset)
{
	long value_y = !strcasecmp(val, "yes");
	long value_n = !strcasecmp(val, "no");

	if (!value_y && !value_n) {
		return "invalid boolean value";
	}

	* (int *) ((char *) *config + offset) = value_y ? 1 : 0;
	return NULL;
}


static int pmf_conf_parse_global_ini(dictionary *ini) {
	int index;
	char *value;
	int section_count;
	char real_key[MAX_KEY_VALUE_LEN];
	int ret = 0;

	/* get global option */
	for (index = 0; index < sizeof(pmf_ini_global_option)/sizeof(INI_VALUE_PARSER_S); index++) {
		real_key[0] = '\0';

		snprintf(real_key, MAX_KEY_VALUE_LEN, "global:%s", pmf_ini_global_option[index].key);
		value = iniparser_getstring(ini, real_key, NULL);
		if (NULL != value) {
			ret = pmf_ini_global_option[index].parser(value, &pmf_global_config, pmf_ini_global_option[index].offset);
			if (0 > ret) {
				plog(PLOG_ERROR, "global option '%s' parse failed!", pmf_ini_global_option[index].key);
				return ret;
			}
		} else {
			plog(PLOG_NOTICE, "global option '%s' doesn't config.", pmf_ini_global_option[index].key);
		}
	}

	/* get pool option */
	section_count = iniparser_getnsec(ini);
	if (-1 == section_count) {
		plog(PLOG_ERROR, "ini file get section count failed!");
		return -1;
	}
	while (section_count > 0) {
		char *section_name = iniparser_getsecname(ini, section_count);

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
				ret = pmf_ini_pool_option[index].parser(value, &pmf_global_config, pmf_ini_pool_option[index].offset);
				if (0 > ret) {
					plog(PLOG_ERROR, "section '%s' option '%s' parse failed!", , pmf_ini_pool_option[index].key);
					return ret;
				}
			} else {
				plog(PLOG_NOTICE, "section '%s' option '%s' doesn't config.", pmf_ini_pool_option[index].key);
			}
		}
		
		section_count--;
	}
	
}

static void pmf_conf_dump() /* {{{ */
{
	plog(PLOG_NOTICE, "[global]");
	plog(PLOG_NOTICE, "\tpid = %s",                         STR2STR(pmf_global_config.pid_file));
	plog(PLOG_NOTICE, "\terror_log = %s",                   STR2STR(pmf_global_config.error_log));
	plog(PLOG_NOTICE, "\tlog_level = %s",                   plog_get_level_name(pmf_global_config.log_level));
	plog(PLOG_NOTICE, "\temergency_restart_interval = %ds", pmf_global_config.emergency_restart_interval);
	plog(PLOG_NOTICE, "\temergency_restart_threshold = %d", pmf_global_config.emergency_restart_threshold);
	plog(PLOG_NOTICE, "\tprocess_control_timeout = %ds",    pmf_global_config.process_control_timeout);
	plog(PLOG_NOTICE, "\tprocess.max = %d",                 pmf_global_config.process_max);
	if (pmf_global_config.process_priority == 64) {
		plog(PLOG_NOTICE, "\tprocess.priority = undefined");
	} else {
		plog(PLOG_NOTICE, "\tprocess.priority = %d", pmf_global_config.process_priority);
	}
	plog(PLOG_NOTICE, "\tdaemonize = %s",                   BOOL2STR(pmf_global_config.daemonize));
	plog(PLOG_NOTICE, "\trlimit_files = %d",                pmf_global_config.rlimit_files);
	plog(PLOG_NOTICE, "\trlimit_core = %d",                 pmf_global_config.rlimit_core);
	plog(PLOG_NOTICE, "\tevents.mechanism = %s",            pmf_global_config.event_mechanism);
	plog(PLOG_NOTICE, " ");
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

int pmf_conf_init_main() {
	int ret;

	ret = pmf_conf_load_conf_file(PMF_CONF_FILE);
	if (0 > ret) {
		plog(PLOG_ERROR, "failed to load configuration file '%s'", PMF_CONF_FILE);
		return -1;
	}

	pmf_conf_dump();
	return ret;
}
