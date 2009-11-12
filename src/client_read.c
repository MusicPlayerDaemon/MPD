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
#include "fifo_buffer.h"

#include <assert.h>
#include <string.h>

static char *
client_read_line(struct client *client)
{
	const char *p, *newline;
	size_t length;
	char *line;

	p = fifo_buffer_read(client->input, &length);
	if (p == NULL)
		return NULL;

	newline = memchr(p, '\n', length);
	if (newline == NULL)
		return NULL;

	line = g_strndup(p, newline - p);
	fifo_buffer_consume(client->input, newline - p + 1);

	return g_strchomp(line);
}

static enum command_return
client_input_received(struct client *client, size_t bytesRead)
{
	char *line;

	fifo_buffer_append(client->input, bytesRead);

	/* process all lines */

	while ((line = client_read_line(client)) != NULL) {
		enum command_return ret = client_process_line(client, line);
		g_free(line);

		if (ret == COMMAND_RETURN_KILL ||
		    ret == COMMAND_RETURN_CLOSE)
			return ret;
		if (client_is_expired(client))
			return COMMAND_RETURN_CLOSE;
	}

	return COMMAND_RETURN_OK;
}

enum command_return
client_read(struct client *client)
{
	char *p;
	size_t max_length;
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_read;

	assert(client != NULL);
	assert(client->channel != NULL);

	p = fifo_buffer_write(client->input, &max_length);
	if (p == NULL) {
		g_warning("[%u] buffer overflow", client->num);
		return COMMAND_RETURN_CLOSE;
	}

	status = g_io_channel_read_chars(client->channel, p, max_length,
					 &bytes_read, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		return client_input_received(client, bytes_read);

	case G_IO_STATUS_AGAIN:
		/* try again later, after select() */
		return COMMAND_RETURN_OK;

	case G_IO_STATUS_EOF:
		/* peer disconnected */
		return COMMAND_RETURN_CLOSE;

	case G_IO_STATUS_ERROR:
		/* I/O error */
		g_warning("failed to read from client %d: %s",
			  client->num, error->message);
		g_error_free(error);
		return COMMAND_RETURN_CLOSE;
	}

	/* unreachable */
	return COMMAND_RETURN_CLOSE;
}
