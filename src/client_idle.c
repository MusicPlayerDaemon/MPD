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
#include "idle.h"

#include <assert.h>

/**
 * Send "idle" response to this client.
 */
static void
client_idle_notify(struct client *client)
{
	unsigned flags, i;
	const char *const* idle_names;

	assert(client->idle_waiting);
	assert(client->idle_flags != 0);

	flags = client->idle_flags;
	client->idle_flags = 0;
	client->idle_waiting = false;

	idle_names = idle_get_names();
	for (i = 0; idle_names[i]; ++i) {
		if (flags & (1 << i) & client->idle_subscriptions)
			client_printf(client, "changed: %s\n",
				      idle_names[i]);
	}

	client_puts(client, "OK\n");
	g_timer_start(client->last_activity);
}

static void
client_idle_callback(gpointer data, gpointer user_data)
{
	struct client *client = data;
	unsigned flags = GPOINTER_TO_UINT(user_data);

	if (client_is_expired(client))
		return;

	client->idle_flags |= flags;
	if (client->idle_waiting
	    && (client->idle_flags & client->idle_subscriptions)) {
		client_idle_notify(client);
		client_write_output(client);
	}
}

void client_manager_idle_add(unsigned flags)
{
	assert(flags != 0);

	client_list_foreach(client_idle_callback, GUINT_TO_POINTER(flags));
}

bool client_idle_wait(struct client *client, unsigned flags)
{
	assert(!client->idle_waiting);

	client->idle_waiting = true;
	client->idle_subscriptions = flags;

	if (client->idle_flags & client->idle_subscriptions) {
		client_idle_notify(client);
		return true;
	} else
		return false;
}
