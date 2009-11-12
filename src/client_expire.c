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

static guint expire_source_id;

void
client_set_expired(struct client *client)
{
	if (!client_is_expired(client))
		client_schedule_expire();

	if (client->source_id != 0) {
		g_source_remove(client->source_id);
		client->source_id = 0;
	}

	if (client->channel != NULL) {
		g_io_channel_unref(client->channel);
		client->channel = NULL;
	}
}

static void
client_check_expired_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct client *client = data;

	if (client_is_expired(client)) {
		g_debug("[%u] expired", client->num);
		client_close(client);
	} else if (!client->idle_waiting && /* idle clients
					       never expire */
		   (int)g_timer_elapsed(client->last_activity, NULL) >
		   client_timeout) {
		g_debug("[%u] timeout", client->num);
		client_close(client);
	}
}

static void
client_manager_expire(void)
{
	client_list_foreach(client_check_expired_callback, NULL);
}

/**
 * An idle event which calls client_manager_expire().
 */
static gboolean
client_manager_expire_event(G_GNUC_UNUSED gpointer data)
{
	expire_source_id = 0;
	client_manager_expire();
	return false;
}

void
client_schedule_expire(void)
{
	if (expire_source_id == 0)
		/* delayed deletion */
		expire_source_id = g_idle_add(client_manager_expire_event,
					      NULL);
}

void
client_deinit_expire(void)
{
	if (expire_source_id != 0)
		g_source_remove(expire_source_id);
}
