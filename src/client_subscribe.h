/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_CLIENT_SUBSCRIBE_H
#define MPD_CLIENT_SUBSCRIBE_H

#include <stdbool.h>
#include <glib.h>

struct client;
struct client_message;

enum client_subscribe_result {
	/** success */
	CLIENT_SUBSCRIBE_OK,

	/** invalid channel name */
	CLIENT_SUBSCRIBE_INVALID,

	/** already subscribed to this channel */
	CLIENT_SUBSCRIBE_ALREADY,

	/** too many subscriptions */
	CLIENT_SUBSCRIBE_FULL,
};

enum client_subscribe_result
client_subscribe(struct client *client, const char *channel);

bool
client_unsubscribe(struct client *client, const char *channel);

void
client_unsubscribe_all(struct client *client);

bool
client_push_message(struct client *client, const struct client_message *msg);

G_GNUC_MALLOC
GSList *
client_read_messages(struct client *client);

#endif
