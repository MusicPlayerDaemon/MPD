// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Daemon.hxx"
#include "PidFile.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/SystemError.hxx"
#include "fs/AllocatedPath.hxx"

#include <csignal>
#include <cstdlib> // for std::exit()
#include <stdexcept>

#include <unistd.h>

/** the absolute path of the pidfile */
static AllocatedPath pidfile = nullptr;

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
daemonize_commit()
{
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
