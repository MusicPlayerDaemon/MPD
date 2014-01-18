/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "PidFile.hxx"
#include "Log.hxx"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef WIN32
#include <sys/wait.h>
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
static gid_t user_gid = (gid_t)-1;

/** the absolute path of the pidfile */
static AllocatedPath pidfile = AllocatedPath::Null();

/* whether "group" conf. option was given */
static bool had_group = false;

/**
 * The write end of a pipe that is used to notify the parent process
 * that initialization has finished and that it should detach.
 */
static int detach_fd = -1;

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
	if (user_gid != (gid_t)-1 && user_gid != getgid() &&
	    setgid(user_gid) == -1) {
		FormatFatalSystemError("Failed to set group %d",
				       (int)user_gid);
	}

#ifdef _BSD_SOURCE
	/* init supplementary groups
	 * (must be done before we change our uid)
	 */
	if (!had_group &&
	    /* no need to set the new user's supplementary groups if
	       we are already this user */
	    user_uid != getuid() &&
	    initgroups(user_name, user_gid) == -1) {
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

void
daemonize_begin(bool detach)
{
	/* release the current working directory */
	if (chdir("/") < 0)
		FatalError("problems changing to root directory");

	if (!detach)
		/* the rest of this function deals with detaching the
		   process */
		return;

	/* do this before daemonizing so we can fail gracefully if we
	   can't write to the pid file */
	PidFile pidfile2(pidfile);

	/* flush all file handles before duplicating the buffers */

	fflush(nullptr);

	/* create a pipe to synchronize the parent and the child */

	int fds[2];
	if (pipe(fds) < 0)
		FatalSystemError("pipe() failed");

	/* move to a child process */

	pid_t pid = fork();
	if (pid < 0)
		FatalSystemError("fork() failed");

	if (pid == 0) {
		/* in the child process */

		pidfile2.Close();
		close(fds[0]);
		detach_fd = fds[1];

		/* detach from the current session */
		setsid();

		/* continue starting MPD */
		return;
	}

	/* in the parent process */

	close(fds[1]);

	int result;
	ssize_t nbytes = read(fds[0], &result, sizeof(result));
	if (nbytes == (ssize_t)sizeof(result)) {
		/* the child process was successful */
		pidfile2.Write(pid);
		exit(EXIT_SUCCESS);
	}

	/* something bad happened in the child process */

	pidfile2.Delete(pidfile);

	int status;
	pid_t pid2 = waitpid(pid, &status, 0);
	if (pid2 < 0)
		FatalSystemError("waitpid() failed");

	if (WIFSIGNALED(status))
		FormatFatalError("MPD died from signal %d%s", WTERMSIG(status),
				 WCOREDUMP(status) ? " (core dumped)" : "");

	exit(WEXITSTATUS(status));
}

void
daemonize_commit()
{
	if (detach_fd >= 0) {
		/* tell the parent process to let go of us and exit
		   indicating success */
		int result = 0;
		write(detach_fd, &result, sizeof(result));
		close(detach_fd);
	} else
		/* the pidfile was not written by the parent because
		   there is no parent - do it now */
		PidFile(pidfile).Write();
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

		user_name = strdup(user);

		/* this is needed by libs such as arts */
		setenv("HOME", pwd->pw_dir, true);
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

	free(user_name);
}

#endif
