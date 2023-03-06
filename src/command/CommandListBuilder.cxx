// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "CommandListBuilder.hxx"
#include "client/Config.hxx"

#include <string.h>

void
CommandListBuilder::Reset()
{
	list.clear();
	mode = Mode::DISABLED;
}

bool
CommandListBuilder::Add(const char *cmd)
{
	size_t len = strlen(cmd) + 1;
	size += len;
	if (size > client_max_command_list_size)
		return false;

	list.emplace_back(cmd);
	return true;
}
