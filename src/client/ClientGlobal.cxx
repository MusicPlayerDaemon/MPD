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
#include "config/ConfigGlobal.hxx"

#define CLIENT_TIMEOUT_DEFAULT			(60)
#define CLIENT_MAX_COMMAND_LIST_DEFAULT		(2048*1024)
#define CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT	(8192*1024)

int client_timeout;
size_t client_max_command_list_size;
size_t client_max_output_buffer_size;

void client_manager_init(void)
{
	client_timeout = config_get_positive(CONF_CONN_TIMEOUT,
					     CLIENT_TIMEOUT_DEFAULT);
	client_max_command_list_size =
		config_get_positive(CONF_MAX_COMMAND_LIST_SIZE,
				    CLIENT_MAX_COMMAND_LIST_DEFAULT / 1024)
		* 1024;

	client_max_output_buffer_size =
		config_get_positive(CONF_MAX_OUTPUT_BUFFER_SIZE,
				    CLIENT_MAX_OUTPUT_BUFFER_SIZE_DEFAULT / 1024)
		* 1024;
}
