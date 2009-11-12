/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "client_internal.h"

#include <string.h>

#define CLIENT_LIST_MODE_BEGIN "command_list_begin"
#define CLIENT_LIST_OK_MODE_BEGIN "command_list_ok_begin"
#define CLIENT_LIST_MODE_END "command_list_end"

static enum command_return
client_process_command_list(struct client *client, bool list_ok, GSList *list)
{
	enum command_return ret = COMMAND_RETURN_OK;
	unsigned num = 0;

	for (GSList *cur = list; cur != NULL; cur = g_slist_next(cur)) {
		char *cmd = cur->data;

		g_debug("command_process_list: process command \"%s\"",
			cmd);
		ret = command_process(client, num++, cmd);
		g_debug("command_process_list: command returned %i", ret);
		if (ret != COMMAND_RETURN_OK || client_is_expired(client))
			break;
		else if (list_ok)
			client_puts(client, "list_OK\n");
	}

	return ret;
}

enum command_return
client_process_line(struct client *client, char *line)
{
	enum command_return ret;

	if (strcmp(line, "noidle") == 0) {
		if (client->idle_waiting) {
			/* send empty idle response and leave idle mode */
			client->idle_waiting = false;
			command_success(client);
			client_write_output(client);
		}

		/* do nothing if the client wasn't idling: the client
		   has already received the full idle response from
		   client_idle_notify(), which he can now evaluate */

		return COMMAND_RETURN_OK;
	} else if (client->idle_waiting) {
		/* during idle mode, clients must not send anything
		   except "noidle" */
		g_warning("[%u] command \"%s\" during idle",
			  client->num, line);
		return COMMAND_RETURN_CLOSE;
	}

	if (client->cmd_list_OK >= 0) {
		if (strcmp(line, CLIENT_LIST_MODE_END) == 0) {
			g_debug("[%u] process command list",
				client->num);

			/* for scalability reasons, we have prepended
			   each new command; now we have to reverse it
			   to restore the correct order */
			client->cmd_list = g_slist_reverse(client->cmd_list);

			ret = client_process_command_list(client,
							  client->cmd_list_OK,
							  client->cmd_list);
			g_debug("[%u] process command "
				"list returned %i", client->num, ret);

			if (ret == COMMAND_RETURN_CLOSE ||
			    client_is_expired(client))
				return COMMAND_RETURN_CLOSE;

			if (ret == COMMAND_RETURN_OK)
				command_success(client);

			client_write_output(client);
			free_cmd_list(client->cmd_list);
			client->cmd_list = NULL;
			client->cmd_list_OK = -1;
		} else {
			size_t len = strlen(line) + 1;
			client->cmd_list_size += len;
			if (client->cmd_list_size >
			    client_max_command_list_size) {
				g_warning("[%u] command list size (%lu) "
					  "is larger than the max (%lu)",
					  client->num,
					  (unsigned long)client->cmd_list_size,
					  (unsigned long)client_max_command_list_size);
				return COMMAND_RETURN_CLOSE;
			}

			new_cmd_list_ptr(client, line);
			ret = COMMAND_RETURN_OK;
		}
	} else {
		if (strcmp(line, CLIENT_LIST_MODE_BEGIN) == 0) {
			client->cmd_list_OK = 0;
			ret = COMMAND_RETURN_OK;
		} else if (strcmp(line, CLIENT_LIST_OK_MODE_BEGIN) == 0) {
			client->cmd_list_OK = 1;
			ret = COMMAND_RETURN_OK;
		} else {
			g_debug("[%u] process command \"%s\"",
				client->num, line);
			ret = command_process(client, 0, line);
			g_debug("[%u] command returned %i",
				client->num, ret);

			if (ret == COMMAND_RETURN_CLOSE ||
			    client_is_expired(client))
				return COMMAND_RETURN_CLOSE;

			if (ret == COMMAND_RETURN_OK)
				command_success(client);

			client_write_output(client);
		}
	}

	return ret;
}
