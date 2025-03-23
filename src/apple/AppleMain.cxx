// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Main.hxx"
#include "Instance.hxx"
#include "net/Init.hxx"
#include "config/Data.hxx"

#ifdef ENABLE_DAEMON
#include "CommandLine.hxx"
#include "cmdline/OptionDef.hxx"
#include "cmdline/OptionParser.hxx"

static constexpr OptionDef option_defs[] = {
	{"no-daemon", "don't detach from console"},
};
#endif

static int service_argc;
static char **service_argv;

int apple_main(int argc, char *argv[])
{
	service_argc = argc;
	service_argv = argv;

#ifdef ENABLE_DAEMON
	OptionParser parser(option_defs, argc, argv);

	if (parser.PeekOptionValue("no-daemon") == nullptr) {
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
