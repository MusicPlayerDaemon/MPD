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

#include "config.h"
#include "ClientList.hxx"
#include "ClientInternal.hxx"

#include <list>
#include <algorithm>

#include <assert.h>

static std::list<Client *> clients;
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

Client *
client_list_get_first(void)
{
	assert(!clients.empty());

	return clients.front();
}

void
client_list_add(Client *client)
{
	clients.push_front(client);
	++num_clients;
}

void
client_list_foreach(void (*callback)(Client *client, void *ctx), void *ctx)
{
	for (Client *client : clients)
		callback(client, ctx);
}

void
client_list_remove(Client *client)
{
	assert(num_clients > 0);
	assert(!clients.empty());

	auto i = std::find(clients.begin(), clients.end(), client);
	assert(i != clients.end());
	clients.erase(i);
	--num_clients;
}
