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
#include "main.h"

#include <assert.h>

static gboolean
client_out_event(G_GNUC_UNUSED GIOChannel *source, GIOCondition condition,
		 gpointer data)
{
	struct client *client = data;

	assert(!client_is_expired(client));

	if (condition != G_IO_OUT) {
		client_set_expired(client);
		return false;
	}

	client_write_deferred(client);

	if (client_is_expired(client)) {
		client_close(client);
		return false;
	}

	g_timer_start(client->last_activity);

	if (g_queue_is_empty(client->deferred_send)) {
		/* done sending deferred buffers exist: schedule
		   read */
		client->source_id = g_io_add_watch(client->channel,
						   G_IO_IN|G_IO_ERR|G_IO_HUP,
						   client_in_event, client);
		return false;
	}

	/* write more */
	return true;
}

gboolean
client_in_event(G_GNUC_UNUSED GIOChannel *source, GIOCondition condition,
		gpointer data)
{
	struct client *client = data;
	enum command_return ret;

	assert(!client_is_expired(client));

	if (condition != G_IO_IN) {
		client_set_expired(client);
		return false;
	}

	g_timer_start(client->last_activity);

	ret = client_read(client);
	switch (ret) {
	case COMMAND_RETURN_OK:
	case COMMAND_RETURN_ERROR:
		break;

	case COMMAND_RETURN_KILL:
		client_close(client);
		g_main_loop_quit(main_loop);
		return false;

	case COMMAND_RETURN_CLOSE:
		client_close(client);
		return false;
	}

	if (client_is_expired(client)) {
		client_close(client);
		return false;
	}

	if (!g_queue_is_empty(client->deferred_send)) {
		/* deferred buffers exist: schedule write */
		client->source_id = g_io_add_watch(client->channel,
						   G_IO_OUT|G_IO_ERR|G_IO_HUP,
						   client_out_event, client);
		return false;
	}

	/* read more */
	return true;
}
