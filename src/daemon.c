/*
 * Copyright (C) 2003-2008 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "daemon.h"
#include "conf.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef WIN32
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#endif

void
daemonize_kill(void)
{
#ifndef WIN32
	FILE *fp;
	struct config_param *param =
		parseConfigFilePath(CONF_PID_FILE, 0);
	int pid, ret;

	if (param == NULL)
		g_error("no pid_file specified in the config file");

	fp = fopen(param->value, "r");
	if (fp == NULL)
		g_error("unable to open %s \"%s\": %s",
			CONF_PID_FILE, param->value, g_strerror(errno));

	if (fscanf(fp, "%i", &pid) != 1) {
		g_error("unable to read the pid from file \"%s\"",
			param->value);
	}
	fclose(fp);

	ret = kill(pid, SIGTERM);
	if (ret < 0)
		g_error("unable to kill proccess %i: %s",
			pid, g_strerror(errno));

	exit(EXIT_SUCCESS);
#else
	g_error("--kill is not available on WIN32");
#endif
}

void
daemonize_close_stdin(void)
{
	int fd = open("/dev/null", O_RDONLY);

	if (fd < 0)
		close(STDIN_FILENO);
	else if (fd != STDIN_FILENO) {
		dup2(fd, STDIN_FILENO);
		close(fd);
	}
}

void
daemonize_set_user(void)
{
#ifndef WIN32
	struct config_param *param = config_get_param(CONF_USER);

	if (param && strlen(param->value)) {
		/* get uid */
		struct passwd *userpwd;
		if ((userpwd = getpwnam(param->value)) == NULL) {
			g_error("no such user \"%s\" at line %i",
				param->value, param->line);
		}

		if (setgid(userpwd->pw_gid) == -1) {
			g_error("cannot setgid for user \"%s\" at line %i: %s",
				param->value, param->line, strerror(errno));
		}
#ifdef _BSD_SOURCE
		/* init suplementary groups
		 * (must be done before we change our uid)
		 */
		if (initgroups(param->value, userpwd->pw_gid) == -1) {
			g_warning("cannot init supplementary groups "
				  "of user \"%s\" at line %i: %s",
				  param->value, param->line, strerror(errno));
		}
#endif

		/* set uid */
		if (setuid(userpwd->pw_uid) == -1) {
			g_error("cannot change to uid of user "
				"\"%s\" at line %i: %s",
				param->value, param->line, strerror(errno));
		}

		/* this is needed by libs such as arts */
		if (userpwd->pw_dir) {
			g_setenv("HOME", userpwd->pw_dir, true);
		}
	}
#endif
}

void
daemonize(Options *options)
{
#ifndef WIN32
	FILE *fp = NULL;
	struct config_param *pidFileParam =
		parseConfigFilePath(CONF_PID_FILE, 0);

	if (pidFileParam) {
		/* do this before daemon'izing so we can fail gracefully if we can't
		 * write to the pid file */
		g_debug("opening pid file");
		fp = fopen(pidFileParam->value, "w+");
		if (!fp) {
			g_error("could not open %s \"%s\" (at line %i) for writing: %s",
				CONF_PID_FILE, pidFileParam->value,
				pidFileParam->line, strerror(errno));
		}
	}

	if (options->daemon) {
		int pid;

		fflush(NULL);
		pid = fork();
		if (pid > 0)
			_exit(EXIT_SUCCESS);
		else if (pid < 0) {
			g_error("problems fork'ing for daemon!");
		}

		if (chdir("/") < 0) {
			g_error("problems changing to root directory");
		}

		setsid();

		g_debug("daemonized!");
	}

	if (pidFileParam) {
		g_debug("writing pid file");
		fprintf(fp, "%lu\n", (unsigned long)getpid());
		fclose(fp);
	}
#else
	/* no daemonization on WIN32 */
	(void)options;
#endif
}

void
daemonize_delete_pidfile(void)
{
	struct config_param *param = parseConfigFilePath(CONF_PID_FILE, 0);

	if (param == NULL)
		return;

	g_debug("cleaning up pid file");

	unlink(param->value);
}
