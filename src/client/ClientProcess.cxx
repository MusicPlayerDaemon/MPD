/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "ClientInternal.hxx"
#include "protocol/Result.hxx"
#include "command/AllCommands.hxx"
#include "Log.hxx"

#include <string.h>

#define CLIENT_LIST_MODE_BEGIN "command_list_begin"
#define CLIENT_LIST_OK_MODE_BEGIN "command_list_ok_begin"
#define CLIENT_LIST_MODE_END "command_list_end"

static CommandResult
client_process_command_list(Client &client, bool list_ok,
			    std::list<std::string> &&list)
{
	CommandResult ret = CommandResult::OK;
	unsigned num = 0;

	for (auto &&i : list) {
		char *cmd = &*i.begin();

		FormatDebug(client_domain, "process command \"%s\"", cmd);
		ret = command_process(client, num++, cmd);
		FormatDebug(client_domain, "command returned %i", int(ret));
		if (ret != CommandResult::OK || client.IsExpired())
			break;
		else if (list_ok)
			client_puts(client, "list_OK\n");
	}

	return ret;
}

CommandResult
client_process_line(Client &client, char *line)
{
	CommandResult ret;

	if (strcmp(line, "noidle") == 0) {
		if (client.idle_waiting) {
			/* send empty idle response and leave idle mode */
			client.idle_waiting = false;
			command_success(client);
		}

		/* do nothing if the client wasn't idling: the client
		   has already received the full idle response from
		   client_idle_notify(), which he can now evaluate */

		return CommandResult::OK;
	} else if (client.idle_waiting) {
		/* during idle mode, clients must not send anything
		   except "noidle" */
		FormatWarning(client_domain,
			      "[%u] command \"%s\" during idle",
			      client.num, line);
		return CommandResult::CLOSE;
	}

	if (client.cmd_list.IsActive()) {
		if (strcmp(line, CLIENT_LIST_MODE_END) == 0) {
			FormatDebug(client_domain,
				    "[%u] process command list",
				    client.num);

			auto &&cmd_list = client.cmd_list.Commit();

			ret = client_process_command_list(client,
							  client.cmd_list.IsOKMode(),
							  std::move(cmd_list));
			FormatDebug(client_domain,
				    "[%u] process command "
				    "list returned %i", client.num, int(ret));

			if (ret == CommandResult::CLOSE ||
			    client.IsExpired())
				return CommandResult::CLOSE;

			if (ret == CommandResult::OK)
				command_success(client);

			client.cmd_list.Reset();
		} else {
			if (!client.cmd_list.Add(line)) {
				FormatWarning(client_domain,
					      "[%u] command list size "
					      "is larger than the max (%lu)",
					      client.num,
					      (unsigned long)client_max_command_list_size);
				return CommandResult::CLOSE;
			}

			ret = CommandResult::OK;
		}
	} else {
		if (strcmp(line, CLIENT_LIST_MODE_BEGIN) == 0) {
			client.cmd_list.Begin(false);
			ret = CommandResult::OK;
		} else if (strcmp(line, CLIENT_LIST_OK_MODE_BEGIN) == 0) {
			client.cmd_list.Begin(true);
			ret = CommandResult::OK;
		} else {
			FormatDebug(client_domain,
				    "[%u] process command \"%s\"",
				    client.num, line);
			ret = command_process(client, 0, line);
			FormatDebug(client_domain,
				    "[%u] command returned %i",
				    client.num, int(ret));

			if (ret == CommandResult::CLOSE ||
			    client.IsExpired())
				return CommandResult::CLOSE;

			if (ret == CommandResult::OK)
				command_success(client);
		}
	}

	return ret;
}
