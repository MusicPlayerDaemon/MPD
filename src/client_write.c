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

#include <assert.h>
#include <string.h>
#include <stdio.h>

static size_t
client_write_deferred_buffer(struct client *client,
			     const struct deferred_buffer *buffer)
{
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_written;

	assert(client != NULL);
	assert(client->channel != NULL);
	assert(buffer != NULL);

	status = g_io_channel_write_chars
		(client->channel, buffer->data, buffer->size,
		 &bytes_written, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		return bytes_written;

	case G_IO_STATUS_AGAIN:
		return 0;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		client_set_expired(client);
		return 0;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		client_set_expired(client);
		g_warning("failed to flush buffer for %i: %s",
			  client->num, error->message);
		g_error_free(error);
		return 0;
	}

	/* unreachable */
	return 0;
}

void
client_write_deferred(struct client *client)
{
	size_t ret;

	while (!g_queue_is_empty(client->deferred_send)) {
		struct deferred_buffer *buf =
			g_queue_peek_head(client->deferred_send);

		assert(buf->size > 0);
		assert(buf->size <= client->deferred_bytes);

		ret = client_write_deferred_buffer(client, buf);
		if (ret == 0)
			break;

		if (ret < buf->size) {
			assert(client->deferred_bytes >= (size_t)ret);
			client->deferred_bytes -= ret;
			buf->size -= ret;
			memmove(buf->data, buf->data + ret, buf->size);
			break;
		} else {
			size_t decr = sizeof(*buf) -
				sizeof(buf->data) + buf->size;

			assert(client->deferred_bytes >= decr);
			client->deferred_bytes -= decr;
			g_free(buf);
			g_queue_pop_head(client->deferred_send);
		}

		g_timer_start(client->last_activity);
	}

	if (g_queue_is_empty(client->deferred_send)) {
		g_debug("[%u] buffer empty %lu", client->num,
			(unsigned long)client->deferred_bytes);
		assert(client->deferred_bytes == 0);
	}
}

static void client_defer_output(struct client *client,
				const void *data, size_t length)
{
	size_t alloc;
	struct deferred_buffer *buf;

	assert(length > 0);

	alloc = sizeof(*buf) - sizeof(buf->data) + length;
	client->deferred_bytes += alloc;
	if (client->deferred_bytes > client_max_output_buffer_size) {
		g_warning("[%u] output buffer size (%lu) is "
			  "larger than the max (%lu)",
			  client->num,
			  (unsigned long)client->deferred_bytes,
			  (unsigned long)client_max_output_buffer_size);
		/* cause client to close */
		client_set_expired(client);
		return;
	}

	buf = g_malloc(alloc);
	buf->size = length;
	memcpy(buf->data, data, length);

	g_queue_push_tail(client->deferred_send, buf);
}

static void client_write_direct(struct client *client,
				const char *data, size_t length)
{
	GError *error = NULL;
	GIOStatus status;
	gsize bytes_written;

	assert(client != NULL);
	assert(client->channel != NULL);
	assert(data != NULL);
	assert(length > 0);
	assert(g_queue_is_empty(client->deferred_send));

	status = g_io_channel_write_chars(client->channel, data, length,
					  &bytes_written, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
	case G_IO_STATUS_AGAIN:
		break;

	case G_IO_STATUS_EOF:
		/* client has disconnected */

		client_set_expired(client);
		return;

	case G_IO_STATUS_ERROR:
		/* I/O error */

		client_set_expired(client);
		g_warning("failed to write to %i: %s",
			  client->num, error->message);
		g_error_free(error);
		return;
	}

	if (bytes_written < length)
		client_defer_output(client, data + bytes_written,
				    length - bytes_written);

	if (!g_queue_is_empty(client->deferred_send))
		g_debug("[%u] buffer created", client->num);
}

void
client_write_output(struct client *client)
{
	if (client_is_expired(client) || !client->send_buf_used)
		return;

	if (!g_queue_is_empty(client->deferred_send)) {
		client_defer_output(client, client->send_buf,
				    client->send_buf_used);

		if (client_is_expired(client))
			return;

		/* try to flush the deferred buffers now; the current
		   server command may take too long to finish, and
		   meanwhile try to feed output to the client,
		   otherwise it will time out.  One reason why
		   deferring is slow might be that currently each
		   client_write() allocates a new deferred buffer.
		   This should be optimized after MPD 0.14. */
		client_write_deferred(client);
	} else
		client_write_direct(client, client->send_buf,
				    client->send_buf_used);

	client->send_buf_used = 0;
}

/**
 * Write a block of data to the client.
 */
static void client_write(struct client *client, const char *buffer, size_t buflen)
{
	/* if the client is going to be closed, do nothing */
	if (client_is_expired(client))
		return;

	while (buflen > 0 && !client_is_expired(client)) {
		size_t copylen;

		assert(client->send_buf_used < sizeof(client->send_buf));

		copylen = sizeof(client->send_buf) - client->send_buf_used;
		if (copylen > buflen)
			copylen = buflen;

		memcpy(client->send_buf + client->send_buf_used, buffer,
		       copylen);
		buflen -= copylen;
		client->send_buf_used += copylen;
		buffer += copylen;
		if (client->send_buf_used >= sizeof(client->send_buf))
			client_write_output(client);
	}
}

void client_puts(struct client *client, const char *s)
{
	client_write(client, s, strlen(s));
}

void client_vprintf(struct client *client, const char *fmt, va_list args)
{
	va_list tmp;
	int length;
	char *buffer;

	va_copy(tmp, args);
	length = vsnprintf(NULL, 0, fmt, tmp);
	va_end(tmp);

	if (length <= 0)
		/* wtf.. */
		return;

	buffer = g_malloc(length + 1);
	vsnprintf(buffer, length + 1, fmt, args);
	client_write(client, buffer, length);
	g_free(buffer);
}

G_GNUC_PRINTF(2, 3) void client_printf(struct client *client, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	client_vprintf(client, fmt, args);
	va_end(args);
}
