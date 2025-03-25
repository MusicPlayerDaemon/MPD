// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Main.hxx"
#include "CommandLine.hxx"
#include "config/Data.hxx"

#include <unistd.h> // for fork()

int apple_main(int argc, char *argv[])
{
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

	MainConfigured(options, raw_config);

	return EXIT_SUCCESS;
#else
	return mpd_main(argc, argv);
#endif
}
