// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Daemon.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "fs/AllocatedPath.hxx"

#ifndef _WIN32
#include "PidFile.hxx"
#endif

#include <fcntl.h>

#ifndef _WIN32
#include <csignal>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#endif

#ifndef WCOREDUMP
#define WCOREDUMP(v) 0
#endif

#ifndef _WIN32

/** the Unix user name which MPD runs as */
static char *user_name;

/** the Unix user id which MPD runs as */
static uid_t user_uid = (uid_t)-1;

/** the Unix group id which MPD runs as */
static gid_t user_gid = (gid_t)-1;

/** the absolute path of the pidfile */
static AllocatedPath pidfile = nullptr;

/* whether "group" conf. option was given */
static bool had_group = false;

/**
 * The write end of a pipe that is used to notify the parent process
 * that initialization has finished and that it should detach.
 */
static int detach_fd = -1;

void
daemonize_kill()
{
	if (pidfile.IsNull())
		throw std::runtime_error("no pid_file specified in the config file");

	const pid_t pid = ReadPidFile(pidfile);
	if (pid < 0)
		throw FmtErrno("unable to read the pid from file \"{}\"",
			       pidfile);

	if (kill(pid, SIGTERM) < 0)
		throw FmtErrno("unable to kill process {}", pid);

	std::exit(EXIT_SUCCESS);
}

void
daemonize_close_stdin()
{
	close(STDIN_FILENO);
	open("/dev/null", O_RDONLY);
}

void
daemonize_set_user()
{
	if (user_name == nullptr)
		return;

	/* set gid */
	if (user_gid != (gid_t)-1 && user_gid != getgid() &&
	    setgid(user_gid) == -1) {
		throw FmtErrno("Failed to set group {}", user_gid);
	}

#ifdef HAVE_INITGROUPS
	/* init supplementary groups
	 * (must be done before we change our uid)
	 */
	if (!had_group &&
	    /* no need to set the new user's supplementary groups if
	       we are already this user */
	    user_uid != getuid() &&
	    initgroups(user_name, user_gid) == -1) {
		throw FmtErrno("Failed to set supplementary groups "
			       "of user \"{}\"",
			       user_name);
	}
#endif

	/* set uid */
	if (user_uid != (uid_t)-1 && user_uid != getuid() &&
	    setuid(user_uid) == -1) {
		throw FmtErrno("Failed to set user \"{}\"", user_name);
	}
}

void
daemonize_begin(bool detach)
{
	/* release the current working directory */
	if (chdir("/") < 0)
		throw MakeErrno("problems changing to root directory");

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
		throw MakeErrno("pipe() failed");

	/* move to a child process */

	pid_t pid = fork();
	if (pid < 0)
		throw MakeErrno("fork() failed");

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
		std::exit(EXIT_SUCCESS);
	}

	/* something bad happened in the child process */

	pidfile2.Delete(pidfile);

	int status;
	pid_t pid2 = waitpid(pid, &status, 0);
	if (pid2 < 0)
		throw MakeErrno("waitpid() failed");

	if (WIFSIGNALED(status))
		throw FmtErrno("MPD died from signal {}{}", WTERMSIG(status),
			       WCOREDUMP(status) ? " (core dumped)" : "");

	std::exit(WEXITSTATUS(status));
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
			throw FmtRuntimeError("no such user \"{}\"", user);

		user_uid = pwd->pw_uid;
		user_gid = pwd->pw_gid;

		user_name = strdup(user);

		/* this is needed by libs such as arts */
		setenv("HOME", pwd->pw_dir, true);
	}

	if (group) {
		struct group *grp = getgrnam(group);
		if (grp == nullptr)
			throw FmtRuntimeError("no such group \"{}\"", group);
		user_gid = grp->gr_gid;
		had_group = true;
	}


	pidfile = std::move(_pidfile);
}

void
daemonize_finish()
{
	if (!pidfile.IsNull()) {
		unlink(pidfile.c_str());
		pidfile = nullptr;
	}

	free(user_name);
}

#endif
