/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "Daemon.hxx"
#include "system/FatalError.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef WIN32
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#endif

static constexpr Domain daemon_domain("daemon");

#ifndef WIN32

/** the Unix user name which MPD runs as */
static char *user_name;

/** the Unix user id which MPD runs as */
static uid_t user_uid = (uid_t)-1;

/** the Unix group id which MPD runs as */
static gid_t user_gid = (pid_t)-1;

/** the absolute path of the pidfile */
static AllocatedPath pidfile = AllocatedPath::Null();

/* whether "group" conf. option was given */
static bool had_group = false;


void
daemonize_kill(void)
{
	FILE *fp;
	int pid, ret;

	if (pidfile.IsNull())
		FatalError("no pid_file specified in the config file");

	fp = FOpen(pidfile, "r");
	if (fp == nullptr) {
		const std::string utf8 = pidfile.ToUTF8();
		FormatFatalSystemError("Unable to open pid file \"%s\"",
				       utf8.c_str());
	}

	if (fscanf(fp, "%i", &pid) != 1) {
		const std::string utf8 = pidfile.ToUTF8();
		FormatFatalError("unable to read the pid from file \"%s\"",
				 utf8.c_str());
	}
	fclose(fp);

	ret = kill(pid, SIGTERM);
	if (ret < 0)
		FormatFatalSystemError("unable to kill process %i",
				       int(pid));

	exit(EXIT_SUCCESS);
}

void
daemonize_close_stdin(void)
{
	close(STDIN_FILENO);
	open("/dev/null", O_RDONLY);
}

void
daemonize_set_user(void)
{
	if (user_name == nullptr)
		return;

	/* set gid */
	if (user_gid != (gid_t)-1 && user_gid != getgid()) {
		if (setgid(user_gid) == -1) {
			FormatFatalSystemError("Failed to set group %d",
					       (int)user_gid);
		}
	}

#ifdef _BSD_SOURCE
	/* init suplementary groups
	 * (must be done before we change our uid)
	 */
	if (!had_group && initgroups(user_name, user_gid) == -1) {
		FormatFatalSystemError("Failed to set supplementary groups "
				       "of user \"%s\"",
				       user_name);
	}
#endif

	/* set uid */
	if (user_uid != (uid_t)-1 && user_uid != getuid() &&
	    setuid(user_uid) == -1) {
		FormatFatalSystemError("Failed to set user \"%s\"",
				       user_name);
	}
}

static void
daemonize_detach(void)
{
	/* flush all file handles before duplicating the buffers */

	fflush(nullptr);

#ifdef HAVE_DAEMON

	if (daemon(0, 1))
		FatalSystemError("daemon() failed");

#elif defined(HAVE_FORK)

	/* detach from parent process */

	switch (fork()) {
	case -1:
		FatalSystemError("fork() failed");
	case 0:
		break;
	default:
		/* exit the parent process */
		_exit(EXIT_SUCCESS);
	}

	/* release the current working directory */

	if (chdir("/") < 0)
		FatalError("problems changing to root directory");

	/* detach from the current session */

	setsid();

#else
	FatalError("no support for daemonizing");
#endif

	LogDebug(daemon_domain, "daemonized");
}

void
daemonize(bool detach)
{
	FILE *fp = nullptr;

	if (!pidfile.IsNull()) {
		/* do this before daemon'izing so we can fail gracefully if we can't
		 * write to the pid file */
		LogDebug(daemon_domain, "opening pid file");
		fp = FOpen(pidfile, "w+");
		if (!fp) {
			const std::string utf8 = pidfile.ToUTF8();
			FormatFatalSystemError("Failed to create pid file \"%s\"",
					       pidfile.c_str());
		}
	}

	if (detach)
		daemonize_detach();

	if (!pidfile.IsNull()) {
		LogDebug(daemon_domain, "writing pid file");
		fprintf(fp, "%lu\n", (unsigned long)getpid());
		fclose(fp);
	}
}

void
daemonize_init(const char *user, const char *group, AllocatedPath &&_pidfile)
{
	if (user) {
		struct passwd *pwd = getpwnam(user);
		if (pwd == nullptr)
			FormatFatalError("no such user \"%s\"", user);

		user_uid = pwd->pw_uid;
		user_gid = pwd->pw_gid;

		user_name = g_strdup(user);

		/* this is needed by libs such as arts */
		g_setenv("HOME", pwd->pw_dir, true);
	}

	if (group) {
		struct group *grp = getgrnam(group);
		if (grp == nullptr)
			FormatFatalError("no such group \"%s\"", group);
		user_gid = grp->gr_gid;
		had_group = true;
	}


	pidfile = std::move(_pidfile);
}

void
daemonize_finish(void)
{
	if (!pidfile.IsNull()) {
		RemoveFile(pidfile);
		pidfile = AllocatedPath::Null();
	}

	g_free(user_name);
}

#endif
