// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ALL_COMMANDS_HXX
#define MPD_ALL_COMMANDS_HXX

#include "CommandResult.hxx"

class Client;

void
command_init() noexcept;

CommandResult
command_process(Client &client, unsigned num, char *line) noexcept;

#endif
