/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "daemon"

#ifndef WIN32

/** the Unix user name which MPD runs as */
static char *user_name;

/** the Unix user id which MPD runs as */
static uid_t user_uid = (uid_t)-1;

/** the Unix group id which MPD runs as */
static gid_t user_gid = (pid_t)-1;

/** the absolute path of the pidfile */
static char *pidfile;

/* whether "group" conf. option was given */
static bool had_group = false;

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
	if (user_name == NULL)
		return;

	/* set gid */
	if (user_gid != (gid_t)-1 && user_gid != getgid()) {
		if (setgid(user_gid) == -1) {
			g_error("cannot setgid to %d: %s",
				(int)user_gid, g_strerror(errno));
		}
	}

#ifdef _BSD_SOURCE
	/* init suplementary groups
	 * (must be done before we change our uid)
	 */
	if (!had_group && initgroups(user_name, user_gid) == -1) {
		g_warning("cannot init supplementary groups "
			  "of user \"%s\": %s",
			  user_name, g_strerror(errno));
	}
#endif

	/* set uid */
	if (user_uid != (uid_t)-1 && user_uid != getuid() &&
	    setuid(user_uid) == -1) {
		g_error("cannot change to uid of user \"%s\": %s",
			user_name, g_strerror(errno));
	}
#endif
}

#ifndef G_OS_WIN32
static void
daemonize_detach(void)
{
	pid_t pid;

	/* flush all file handles before duplicating the buffers */

	fflush(NULL);

	/* detach from parent process */

	pid = fork();
	if (pid < 0)
		g_error("fork() failed: %s", g_strerror(errno));

	if (pid > 0)
		/* exit the parent process */
		_exit(EXIT_SUCCESS);

	/* release the current working directory */

	if (chdir("/") < 0)
		g_error("problems changing to root directory");

	/* detach from the current session */

	setsid();

	g_debug("daemonized!");
}
#endif

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

	if (detach)
		daemonize_detach();

	if (pidfile != NULL) {
		g_debug("writing pid file");
		fprintf(fp, "%lu\n", (unsigned long)getpid());
		fclose(fp);
	}
#else
	/* no daemonization on WIN32 */
	(void)detach;
#endif
}

void
daemonize_init(const char *user, const char *group, const char *_pidfile)
{
#ifndef WIN32
	if (user) {
		struct passwd *pwd = getpwnam(user);
		if (!pwd)
			g_error("no such user \"%s\"", user);

		user_uid = pwd->pw_uid;
		user_gid = pwd->pw_gid;

		user_name = g_strdup(user);

		/* this is needed by libs such as arts */
		g_setenv("HOME", pwd->pw_dir, true);
	}

	if (group) {
		struct group *grp = grp = getgrnam(group);
		if (!grp)
			g_error("no such group \"%s\"", group);
		user_gid = grp->gr_gid;
		had_group = true;
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
