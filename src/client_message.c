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

#include "client_message.h"

#include <assert.h>
#include <glib.h>

G_GNUC_PURE
static bool
valid_channel_char(const char ch)
{
	return g_ascii_isalnum(ch) ||
		ch == '_' || ch == '-' || ch == '.' || ch == ':';
}

bool
client_message_valid_channel_name(const char *name)
{
	do {
		if (!valid_channel_char(*name))
			return false;
	} while (*++name != 0);

	return true;
}

void
client_message_init_null(struct client_message *msg)
{
	assert(msg != NULL);

	msg->channel = NULL;
	msg->message = NULL;
}

void
client_message_init(struct client_message *msg,
		    const char *channel, const char *message)
{
	assert(msg != NULL);

	msg->channel = g_strdup(channel);
	msg->message = g_strdup(message);
}

void
client_message_copy(struct client_message *dest,
		    const struct client_message *src)
{
	assert(dest != NULL);
	assert(src != NULL);
	assert(client_message_defined(src));

	client_message_init(dest, src->channel, src->message);
}

struct client_message *
client_message_dup(const struct client_message *src)
{
	struct client_message *dest = g_slice_new(struct client_message);
	client_message_copy(dest, src);
	return dest;
}

void
client_message_deinit(struct client_message *msg)
{
	assert(msg != NULL);

	g_free(msg->channel);
	g_free(msg->message);
}

void
client_message_free(struct client_message *msg)
{
	client_message_deinit(msg);
	g_slice_free(struct client_message, msg);
}
