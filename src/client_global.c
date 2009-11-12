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
#include "conf.h"

#include <assert.h>

#define CLIENT_TIMEOUT_DEFAULT			(60)
#define CLIENT_MAX_CONNECTIONS_DEFAULT		(10)
#define CLIENT_MAX_COMMAND_LIST_DEFAULT		(2048*1024)
#define CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT	(8192*1024)

/* set this to zero to indicate we have no possible clients */
unsigned int client_max_connections;
int client_timeout;
size_t client_max_command_list_size;
size_t client_max_output_buffer_size;

void client_manager_init(void)
{
	client_timeout = config_get_positive(CONF_CONN_TIMEOUT,
					     CLIENT_TIMEOUT_DEFAULT);
	client_max_connections =
		config_get_positive(CONF_MAX_CONN,
				    CLIENT_MAX_CONNECTIONS_DEFAULT);
	client_max_command_list_size =
		config_get_positive(CONF_MAX_COMMAND_LIST_SIZE,
				    CLIENT_MAX_COMMAND_LIST_DEFAULT / 1024)
		* 1024;

	client_max_output_buffer_size =
		config_get_positive(CONF_MAX_OUTPUT_BUFFER_SIZE,
				    CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT / 1024)
		* 1024;
}

static void client_close_all(void)
{
	while (!client_list_is_empty()) {
		struct client *client = client_list_get_first();

		client_close(client);
	}

	assert(client_list_is_empty());
}

void client_manager_deinit(void)
{
	client_close_all();

	client_max_connections = 0;

	client_deinit_expire();
}
