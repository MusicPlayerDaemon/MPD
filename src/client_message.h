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

#ifndef MPD_CLIENT_MESSAGE_H
#define MPD_CLIENT_MESSAGE_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <glib.h>

/**
 * A client-to-client message.
 */
struct client_message {
	char *channel;

	char *message;
};

G_GNUC_PURE
bool
client_message_valid_channel_name(const char *name);

G_GNUC_PURE
static inline bool
client_message_defined(const struct client_message *msg)
{
	assert(msg != NULL);
	assert((msg->channel == NULL) == (msg->message == NULL));

	return msg->channel != NULL;
}

void
client_message_init_null(struct client_message *msg);

void
client_message_init(struct client_message *msg,
		    const char *channel, const char *message);

void
client_message_copy(struct client_message *dest,
		    const struct client_message *src);

G_GNUC_MALLOC
struct client_message *
client_message_dup(const struct client_message *src);

void
client_message_deinit(struct client_message *msg);

void
client_message_free(struct client_message *msg);

#endif
