// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Main.hxx"
#include "Instance.hxx"
#include "CommandLine.hxx"
#include "net/Init.hxx"
#include "config/Data.hxx"

static int service_argc;
static char **service_argv;

int apple_main(int argc, char *argv[])
{
	service_argc = argc;
	service_argv = argv;

#ifdef ENABLE_DAEMON
	CommandLineOptions options;
	ConfigData raw_config;

	ParseCommandLine(argc, argv, options, raw_config);

	if (options.daemon) {
		// Fork before any Objective-C runtime initializations
		pid_t pid = fork();
		if (pid < 0)
			throw MakeErrno("fork() failed");

		if (pid > 0) {
			// Parent process: exit immediately
			_exit(0);
		}
	}
#endif

	return mpd_main(argc, argv);
}
