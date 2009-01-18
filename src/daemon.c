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

#include <glib.h>

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

#ifndef WIN32

/** the Unix user name which MPD runs as */
static char *user_name;

/** the Unix user id which MPD runs as */
static uid_t user_uid;

/** the Unix group id which MPD runs as */
static gid_t user_gid;

/** the absolute path of the pidfile */
static char *pidfile;

#endif

void
daemonize_kill(void)
{
#ifndef WIN32
	FILE *fp;
	int pid, ret;

	if (pidfile == NULL)
		g_error("no pid_file specified in the config file");

	fp = fopen(pidfile, "r");
	if (fp == NULL)
		g_error("unable to open pid file \"%s\": %s",
			pidfile, g_strerror(errno));

	if (fscanf(fp, "%i", &pid) != 1) {
		g_error("unable to read the pid from file \"%s\"",
			pidfile);
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
	if (user_name != NULL) {
		/* get uid */
		if (setgid(user_gid) == -1) {
			g_error("cannot setgid for user \"%s\": %s",
				user_name, g_strerror(errno));
		}
#ifdef _BSD_SOURCE
		/* init suplementary groups
		 * (must be done before we change our uid)
		 */
		if (initgroups(user_name, user_gid) == -1) {
			g_warning("cannot init supplementary groups "
				  "of user \"%s\": %s",
				  user_name, g_strerror(errno));
		}
#endif

		/* set uid */
		if (setuid(user_uid) == -1) {
			g_error("cannot change to uid of user \"%s\": %s",
				user_name, g_strerror(errno));
		}
	}
#endif
}

void
daemonize(bool detach)
{
#ifndef WIN32
	FILE *fp = NULL;

	if (pidfile != NULL) {
		/* do this before daemon'izing so we can fail gracefully if we can't
		 * write to the pid file */
		g_debug("opening pid file");
		fp = fopen(pidfile, "w+");
		if (!fp) {
			g_error("could not create pid file \"%s\": %s",
				pidfile, g_strerror(errno));
		}
	}

	if (detach) {
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

	if (pidfile != NULL) {
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
daemonize_init(const char *user, const char *_pidfile)
{
#ifndef WIN32
	user_name = g_strdup(user);
	if (user_name != NULL) {
		struct passwd *pwd = getpwnam(user_name);
		if (pwd == NULL)
			g_error("no such user \"%s\"", user_name);

		user_uid = pwd->pw_uid;
		user_gid = pwd->pw_gid;

		/* this is needed by libs such as arts */
		g_setenv("HOME", pwd->pw_dir, true);
	}

	pidfile = g_strdup(_pidfile);
#else
	(void)user;
	(void)_pidfile;
#endif
}

void
daemonize_finish(void)
{
#ifndef WIN32
	if (pidfile != NULL)
		unlink(pidfile);

	g_free(user_name);
	g_free(pidfile);
#endif
}
