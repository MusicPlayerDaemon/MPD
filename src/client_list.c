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

#include <assert.h>

static GList *clients;
static unsigned num_clients;

bool
client_list_is_empty(void)
{
	return num_clients == 0;
}

bool
client_list_is_full(void)
{
	return num_clients >= client_max_connections;
}

struct client *
client_list_get_first(void)
{
	assert(clients != NULL);

	return clients->data;
}

void
client_list_add(struct client *client)
{
	clients = g_list_prepend(clients, client);
	++num_clients;
}

void
client_list_foreach(GFunc func, gpointer user_data)
{
	g_list_foreach(clients, func, user_data);
}

void
client_list_remove(struct client *client)
{
	assert(num_clients > 0);
	assert(clients != NULL);

	clients = g_list_remove(clients, client);
	--num_clients;
}
