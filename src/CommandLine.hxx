// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_COMMAND_LINE_HXX
#define MPD_COMMAND_LINE_HXX

#include "config.h" // for ENABLE_DAEMON

struct ConfigData;

struct CommandLineOptions {
	bool kill = false;

#ifdef ENABLE_DAEMON
	bool daemon = true;
#else
	static constexpr bool daemon = false;
#endif

#ifdef __linux__
	bool systemd = false;
#endif

	bool log_stderr = false;
	bool verbose = false;
};

void
ParseCommandLine(int argc, char **argv, CommandLineOptions &options,
		 ConfigData &config);

#endif
