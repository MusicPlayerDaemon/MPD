// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_COMMAND_LINE_HXX
#define MPD_COMMAND_LINE_HXX

struct ConfigData;

struct CommandLineOptions {
	bool log_stderr = false;
	bool verbose = false;
};

void
ParseCommandLine(int argc, char **argv, CommandLineOptions &options,
		 ConfigData &config);

#endif
