#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "pmf.h"
#include "pmf_conf.h"
#include "pmf_log.h"
#include "pmf_events.h"
#include "pmf_sockets.h"
#include "pmf_worker_pool.h"
#include "pmf_clock.h"

static int pmf_unix_conf_wp(PMF_WORKER_POOL_S *wp) {
	struct passwd *pwd;
	int is_root = !geteuid();

	if (is_root) {
		if (wp->config->user && *wp->config->user) {
			if (strlen(wp->config->user) == strspn(wp->config->user, "0123456789")) {
				wp->set_uid = strtoul(wp->config->user, 0, 10);
			} else {
				struct passwd *pwd;

				pwd = getpwnam(wp->config->user);
				if (!pwd) {
					plog(PLOG_ERROR, "[pool %s] cannot get uid for user '%s'", wp->config->name, wp->config->user);
					return -1;
				}

				wp->set_uid = pwd->pw_uid;
				wp->set_gid = pwd->pw_gid;

				wp->user = strdup(pwd->pw_name);
				wp->home = strdup(pwd->pw_dir);
			}
		}

		if (wp->config->group && *wp->config->group) {
			if (strlen(wp->config->group) == strspn(wp->config->group, "0123456789")) {
				wp->set_gid = strtoul(wp->config->group, 0, 10);
			} else {
				struct group *grp;

				grp = getgrnam(wp->config->group);
				if (!grp) {
					plog(PLOG_ERROR, "[pool %s] cannot get gid for group '%s'", wp->config->name, wp->config->group);
					return -1;
				}
				wp->set_gid = grp->gr_gid;
			}
		}
	} else { /* not root */
		if (wp->config->user && *wp->config->user) {
			plog(PLOG_NOTICE, "[pool %s] 'user' directive is ignored when PMF is not running as root", wp->config->name);
		}
		if (wp->config->group && *wp->config->group) {
			plog(PLOG_NOTICE, "[pool %s] 'group' directive is ignored when PMF is not running as root", wp->config->name);
		}
		if (wp->config->process_priority != 64) {
			plog(PLOG_NOTICE, "[pool %s] 'process.priority' directive is ignored when PMF is not running as root", wp->config->name);
		}

		/* set up HOME and USER anyway */
		pwd = getpwuid(getuid());
		if (pwd) {
			wp->user = strdup(pwd->pw_name);
			wp->home = strdup(pwd->pw_dir);
		}
	}
	return 0;
}

int pmf_unix_set_socket_premissions(PMF_WORKER_POOL_S *wp, const char *path) {
	if (wp->socket_uid != -1 || wp->socket_gid != -1) {
		if (0 > chown(path, wp->socket_uid, wp->socket_gid)) {
			plog(PLOG_SYSERROR, "[pool %s] failed to chown() the socket '%s'", wp->config->name, wp->config->listen_address);
			return -1;
		}
	}
	return 0;
}

int pmf_unix_resolve_socket_premissions(PMF_WORKER_POOL_S *wp) {
	PMF_WORKER_POOL_CONFIG_S *c = wp->config;

	wp->socket_uid = -1;
	wp->socket_gid = -1;
	wp->socket_mode = 0660;

	if (!c) {
		return 0;
	}

	if (c->listen_mode && *c->listen_mode) {
		wp->socket_mode = strtoul(c->listen_mode, 0, 8);
	}

	if (c->listen_owner && *c->listen_owner) {
		struct passwd *pwd;

		pwd = getpwnam(c->listen_owner);
		if (!pwd) {
			plog(PLOG_SYSERROR, "[pool %s] cannot get uid for user '%s'", wp->config->name, c->listen_owner);
			return -1;
		}

		wp->socket_uid = pwd->pw_uid;
		wp->socket_gid = pwd->pw_gid;
	}

	if (c->listen_group && *c->listen_group) {
		struct group *grp;

		grp = getgrnam(c->listen_group);
		if (!grp) {
			plog(PLOG_SYSERROR, "[pool %s] cannot get gid for group '%s'", wp->config->name, c->listen_group);
			return -1;
		}
		wp->socket_gid = grp->gr_gid;
	}

	return 0;
}

int pmf_unix_init_main() {
	PMF_WORKER_POOL_S *wp;
	struct rlimit r;
	int is_root = !geteuid();

	if (pmf_global_config.rlimit_files > 0) {
		r.rlim_max = r.rlim_cur = (rlim_t) pmf_global_config.rlimit_files;

		if (0 > setrlimit(RLIMIT_NOFILE, &r)) {
			plog(PLOG_SYSERROR, "failed to set rlimit_core for this pool. Please check your system limits or decrease rlimit_files. setrlimit(RLIMIT_NOFILE, %d)", pmf_global_config.rlimit_files);
			return -1;
		}
	}

	if (pmf_global_config.rlimit_core) {
		struct rlimit r;

		r.rlim_max = r.rlim_cur = pmf_global_config.rlimit_core == -1 ? (rlim_t) RLIM_INFINITY : (rlim_t) pmf_global_config.rlimit_core;

		if (0 > setrlimit(RLIMIT_CORE, &r)) {
			plog(PLOG_SYSERROR, "failed to set rlimit_core for this pool. Please check your system limits or decrease rlimit_core. setrlimit(RLIMIT_CORE, %d)", pmf_global_config.rlimit_core);
			return -1;
		}
	}

	if (pmf_global_config.daemonize) {
		struct timeval tv;
		fd_set rfds;
		pid_t pid;
		int ret;

		if (-1 == pipe(pmf_globals.send_config_pipe)) {
			plog(PLOG_SYSERROR, "failed to create pipe");
			return -1;
		}

		pid = fork();
		switch (pid) {
			case -1:
				plog(PLOG_SYSERROR, "failed to daemonize");
				return -1;
			case 0:
				close(pmf_globals.send_config_pipe[0]); /* close the read side of the pipe */
				break;
			default:
				/*
				 * wait for 10s before exiting with error
				 * the child is supposed to send 1 or 0 into the pipe to tell the parent
				 * how it goes for it
				 */
				FD_ZERO(&rfds);
				FD_SET(pmf_globals.send_config_pipe[0], &rfds);

				tv.tv_sec = 10;
				tv.tv_usec = 0;

				plog(PLOG_DEBUG, "The calling process is waiting for the master process to ping via fd=%d", pmf_globals.send_config_pipe[0]);
				ret = select(pmf_globals.send_config_pipe[0] + 1, &rfds, NULL, NULL, &tv);
				if (ret == -1) {
					plog(PLOG_SYSERROR, "failed to select");
					return -1;
				}
				if (ret) { /* data available */
					int readval;
					ret = read(pmf_globals.send_config_pipe[0], &readval, sizeof(readval));
					if (ret == -1) {
						plog(PLOG_SYSERROR, "failed to read from pipe");
						return -1;
					}

					if (ret == 0) {
						plog(PLOG_ERROR, "no data have been read from pipe");
						return -1;
					} else {
						if (readval == 1) {
							plog(PLOG_DEBUG, "I received a valid acknoledge from the master process, I can exit without error");
							//pmf_cleanups_run(PMF_CLEANUP_PARENT_EXIT);
							return 0;
						} else {
							plog(PLOG_DEBUG, "The master process returned an error !");
							return -1;
						}
					}
				} else {
					plog(PLOG_ERROR, "the master process didn't send back its status (via the pipe to the calling process)");
				  return -1;
				}
				return -1;
		}
	}

	/* continue as a child */
	setsid();
	if (0 > pmf_clock_init()) {
		return -1;
	}

	if (pmf_global_config.process_priority != 64) {
		if (is_root) {
			if (setpriority(PRIO_PROCESS, 0, pmf_global_config.process_priority) < 0) {
				plog(PLOG_SYSERROR, "Unable to set priority for the master process");
				return -1;
			}
		} else {
			plog(PLOG_NOTICE, "'process.priority' directive is ignored when PMf is not running as root");
		}
	}

	pmf_globals.parent_pid = getpid();
	for (wp = pmf_worker_all_pools; wp; wp = wp->next) {
		if (0 > pmf_unix_conf_wp(wp)) {
			return -1;
		}
	}

	return 0;
}

