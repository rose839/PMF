
#include "pmf_log.h"
#include "pmf_events.h"
#include "plog.h"
#include "iniparser.h"

static INI_VALUE_PARSER_S pmf_ini_global_option[] = {
	{"pid_file", pmf_config_set_string, GO(pid_file)},
};

static INI_VALUE_PARSER_S pmf_ini_pool_option[] = {
	
};

PMF_GLOBAL_CONFIG_S pmf_global_config = {
	.daemonize = 0,
	.process_max = 0,
	.process_priority = 64, /* 64 means unset */
};

static int pmf_config_set_string(dictionary *ini, char *value, void **config, int offset) {
	char *value;
	
	value = iniparser_getstring(ini, key, NULL);
	if (value) {
		
	}
}

static int pmf_conf_parse_ini(dictionary *ini) {
	int index;
	char *value;
	int ret = 0;

	/* get global option */
	for (index = 0; index < sizeof(pmf_ini_global_option)/sizeof(INI_VALUE_PARSER_S); index++) {
		value = iniparser_getstring(ini, pmf_ini_global_option[index].key, NULL);
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
	for (index = 0; index < sizeof(pmf_ini_pool_option)/sizeof(INI_VALUE_PARSER_S); index++) {

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

	if (0 > pmf_conf_parse_ini(ini)) {
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

	return ret;
}
