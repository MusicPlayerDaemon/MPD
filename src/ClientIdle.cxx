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
#include "ClientIdle.hxx"
#include "ClientInternal.hxx"
#include "ClientList.hxx"
#include "Idle.hxx"

#include <assert.h>

void
Client::IdleNotify()
{
	assert(idle_waiting);
	assert(idle_flags != 0);

	unsigned flags = idle_flags;
	idle_flags = 0;
	idle_waiting = false;

	const char *const*idle_names = idle_get_names();
	for (unsigned i = 0; idle_names[i]; ++i) {
		if (flags & (1 << i) & idle_subscriptions)
			client_printf(this, "changed: %s\n",
				      idle_names[i]);
	}

	client_puts(this, "OK\n");
	g_timer_start(last_activity);
}

void
Client::IdleAdd(unsigned flags)
{
	if (IsExpired())
		return;

	idle_flags |= flags;
	if (idle_waiting && (idle_flags & idle_subscriptions))
		IdleNotify();
}

static void
client_idle_callback(Client *client, gpointer user_data)
{
	unsigned flags = GPOINTER_TO_UINT(user_data);

	client->IdleAdd(flags);
}

void client_manager_idle_add(unsigned flags)
{
	assert(flags != 0);

	client_list_foreach(client_idle_callback, GUINT_TO_POINTER(flags));
}

bool
Client::IdleWait(unsigned flags)
{
	assert(!idle_waiting);

	idle_waiting = true;
	idle_subscriptions = flags;

	if (idle_flags & idle_subscriptions) {
		IdleNotify();
		return true;
	} else
		return false;
}
