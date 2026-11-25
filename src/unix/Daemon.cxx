// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Daemon.hxx"
#include "PidFile.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "fs/AllocatedPath.hxx"
#include "io/FileDescriptor.hxx"
#include "util/SpanCast.hxx"

#include <csignal>
#include <cstdlib> // for std::exit()
#include <stdexcept>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef WCOREDUMP
#define WCOREDUMP(v) 0
#endif

/** the absolute path of the pidfile */
static AllocatedPath pidfile = nullptr;

/**
 * The write end of a pipe that is used to notify the parent process
 * that initialization has finished and that it should detach.
 */
static FileDescriptor detach_fd = FileDescriptor::Undefined();

void
daemonize_kill()
{
	if (pidfile.IsNull())
		throw std::runtime_error("no pid_file specified in the config file");

	const pid_t pid = ReadPidFile(pidfile);
	if (pid < 0)
		throw FmtErrno("unable to read the pid from file {:?}",
			       pidfile);

	if (pid <= 1)
		throw FmtRuntimeError("bogus pid {} in file {:?}",
				      pid, pidfile);

	if (kill(pid, SIGTERM) < 0)
		throw FmtErrno("unable to kill process {}", pid);

	std::exit(EXIT_SUCCESS);
}

void
daemonize_close_stdin() noexcept
{
	close(STDIN_FILENO);
	open("/dev/null", O_RDONLY);
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

#ifdef __APPLE__
	/* on macOS, MainOrThrow() has already forked (before any
	   Objective-C runtime initialization) and the parent process
	   has exited; there is no parent process waiting for the
	   pipe, so leave #detach_fd undefined, and
	   daemonize_commit() will write the pid file */

	pidfile2.Close();

	/* detach from the current session */
	setsid();
#else
	/* flush all file handles before duplicating the buffers */

	fflush(nullptr);

	/* create a pipe to synchronize the parent and the child */

	FileDescriptor p;

	if (!FileDescriptor::CreatePipe(p, detach_fd))
		throw MakeErrno("pipe() failed");

	/* move to a child process */

	pid_t pid = fork();
	if (pid < 0)
		throw MakeErrno("fork() failed");

	if (pid == 0) {
		/* in the child process */

		pidfile2.Close();
		p.Close();

		/* detach from the current session */
		setsid();

		/* continue starting MPD */
		return;
	}

	/* in the parent process */

	detach_fd.Close();

	int result;
	ssize_t nbytes = p.Read(ReferenceAsWritableBytes(result));
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

#endif
}

void
daemonize_commit()
{
	if (detach_fd.IsDefined()) {
		/* tell the parent process to let go of us and exit
		   indicating success */
		static constexpr int result = 0;
		(void)detach_fd.Write(ReferenceAsBytes(result));
		detach_fd.Close();
	} else
		/* the pidfile was not written by the parent because
		   there is no parent - do it now */
		PidFile(pidfile).Write();
}

void
daemonize_init(const char *user, const char *group, AllocatedPath &&_pidfile)
{
	if ((user != nullptr || group != nullptr) && geteuid() == 0)
		throw std::runtime_error{"The settings \"user\" and \"group\" have been removed; "
					 "please launch MPD as an unprivileged user"};

	pidfile = std::move(_pidfile);
}

void
daemonize_finish() noexcept
{
	if (!pidfile.IsNull()) {
		unlink(pidfile.c_str());
		pidfile = nullptr;
	}
}
