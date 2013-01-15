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

#include <assert.h>
#include <string.h>
#include <stdio.h>

static size_t
client_write_direct(Client *client, const void *data, size_t length)
{
	assert(client != NULL);
	assert(client->channel != NULL);
	assert(data != NULL);
	assert(length > 0);

	gsize bytes_written;
	GError *error = NULL;
	GIOStatus status =
		g_io_channel_write_chars(client->channel, (const gchar *)data,
					 length, &bytes_written, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		return bytes_written;

	case G_IO_STATUS_AGAIN:
		return 0;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		client->SetExpired();
		return 0;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		client->SetExpired();
		g_warning("failed to write to %i: %s",
			  client->num, error->message);
		g_error_free(error);
		return 0;
	}

	/* unreachable */
	assert(false);
	return 0;
}

void
client_write_deferred(Client *client)
{
	assert(!client_is_expired(client));

	while (true) {
		size_t length;
		const void *data = client->output_buffer.Read(&length);
		if (data == nullptr)
			break;

		size_t nbytes = client_write_direct(client, data, length);
		if (nbytes == 0)
			return;

		client->output_buffer.Consume(nbytes);

		if (nbytes < length)
			return;

		g_timer_start(client->last_activity);
	}
}

static void
client_defer_output(Client *client, const void *data, size_t length)
{
	if (!client->output_buffer.Append(data, length)) {
		g_warning("[%u] output buffer size is "
			  "larger than the max (%lu)",
			  client->num,
			  (unsigned long)client_max_output_buffer_size);
		/* cause client to close */
		client->SetExpired();
		return;
	}
}

void
client_write_output(Client *client)
{
	if (client->IsExpired())
		return;

	client_write_deferred(client);
}

/**
 * Write a block of data to the client.
 */
static void
client_write(Client *client, const char *data, size_t length)
{
	/* if the client is going to be closed, do nothing */
	if (client->IsExpired() || length == 0)
		return;

	client_defer_output(client, data, length);
}

void
client_puts(Client *client, const char *s)
{
	client_write(client, s, strlen(s));
}

void
client_vprintf(Client *client, const char *fmt, va_list args)
{
#ifndef G_OS_WIN32
	va_list tmp;
	int length;

	va_copy(tmp, args);
	length = vsnprintf(NULL, 0, fmt, tmp);
	va_end(tmp);

	if (length <= 0)
		/* wtf.. */
		return;

	char *buffer = (char *)g_malloc(length + 1);
	vsnprintf(buffer, length + 1, fmt, args);
	client_write(client, buffer, length);
	g_free(buffer);
#else
	/* On mingw32, snprintf() expects a 64 bit integer instead of
	   a "long int" for "%li".  This is not consistent with our
	   expectation, so we're using plain sprintf() here, hoping
	   the static buffer is large enough.  Sorry for this hack,
	   but WIN32 development is so painful, I'm not in the mood to
	   do it properly now. */

	static char buffer[4096];
	vsprintf(buffer, fmt, args);
	client_write(client, buffer, strlen(buffer));
#endif
}

G_GNUC_PRINTF(2, 3)
void
client_printf(Client *client, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	client_vprintf(client, fmt, args);
	va_end(args);
}
