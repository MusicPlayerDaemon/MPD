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
#include "ClientInternal.hxx"
#include "Main.hxx"
#include "event/Loop.hxx"

#include <assert.h>

static gboolean
client_out_event(G_GNUC_UNUSED GIOChannel *source, GIOCondition condition,
		 gpointer data)
{
	Client *client = (Client *)data;

	assert(!client->IsExpired());

	if (condition != G_IO_OUT) {
		client->SetExpired();
		return false;
	}

	client_write_deferred(client);

	if (client->IsExpired()) {
		client->Close();
		return false;
	}

	g_timer_start(client->last_activity);

	if (client->output_buffer.IsEmpty()) {
		/* done sending deferred buffers exist: schedule
		   read */
		client->source_id = g_io_add_watch(client->channel,
						   GIOCondition(G_IO_IN|G_IO_ERR|G_IO_HUP),
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
	Client *client = (Client *)data;
	enum command_return ret;

	assert(!client->IsExpired());

	if (condition != G_IO_IN) {
		client->SetExpired();
		return false;
	}

	g_timer_start(client->last_activity);

	ret = client_read(client);
	switch (ret) {
	case COMMAND_RETURN_OK:
	case COMMAND_RETURN_IDLE:
	case COMMAND_RETURN_ERROR:
		break;

	case COMMAND_RETURN_KILL:
		client->Close();
		main_loop->Break();
		return false;

	case COMMAND_RETURN_CLOSE:
		client->Close();
		return false;
	}

	if (client->IsExpired()) {
		client->Close();
		return false;
	}

	if (!client->output_buffer.IsEmpty()) {
		/* deferred buffers exist: schedule write */
		client->source_id = g_io_add_watch(client->channel,
						   GIOCondition(G_IO_OUT|G_IO_ERR|G_IO_HUP),
						   client_out_event, client);
		return false;
	}

	/* read more */
	return true;
}
