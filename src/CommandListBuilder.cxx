/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "CommandListBuilder.hxx"
#include "ClientInternal.hxx"

#include <string.h>

void
CommandListBuilder::Reset()
{
	for (GSList *tmp = cmd_list; tmp != NULL; tmp = g_slist_next(tmp))
		g_free(tmp->data);

	g_slist_free(cmd_list);

	cmd_list = nullptr;
	cmd_list_OK = -1;
}

bool
CommandListBuilder::Add(const char *cmd)
{
	size_t len = strlen(cmd) + 1;
	cmd_list_size += len;
	if (cmd_list_size > client_max_command_list_size)
		return false;

	cmd_list = g_slist_prepend(cmd_list, g_strdup(cmd));
	return true;
}
