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
CommandListBuilder::Add(std::string_view cmd) noexcept
{
	/* count the std::list node (two pointers) and the std::string
	   object, not just the string data */
	size += 2 * sizeof(void *) + sizeof(std::string) + cmd.size() + 1;
	if (size > client_max_command_list_size)
		return false;

	list.emplace_back(cmd);
	return true;
}
