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

#include "tcp_socket.h"
#include "fifo_buffer.h"
#include "io_thread.h"
#include "glib_socket.h"

#include <assert.h>
#include <string.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

struct tcp_socket {
	const struct tcp_socket_handler *handler;
	void *handler_ctx;

	GMutex *mutex;

	GIOChannel *channel;
	GSource *in_source, *out_source;

	struct fifo_buffer *input, *output;
};

static gboolean
tcp_event(GIOChannel *source, GIOCondition condition, gpointer data);

static void
tcp_socket_schedule_read(struct tcp_socket *s)
{
	assert(s->input != NULL);
	assert(!fifo_buffer_is_full(s->input));

	if (s->in_source != NULL)
		return;

	s->in_source = g_io_create_watch(s->channel,
					 G_IO_IN|G_IO_ERR|G_IO_HUP);
	g_source_set_callback(s->in_source, (GSourceFunc)tcp_event, s, NULL);
	g_source_attach(s->in_source, io_thread_context());
}

static void
tcp_socket_unschedule_read(struct tcp_socket *s)
{
	if (s->in_source == NULL)
		return;

	g_source_destroy(s->in_source);
	g_source_unref(s->in_source);
	s->in_source = NULL;
}

static void
tcp_socket_schedule_write(struct tcp_socket *s)
{
	assert(s->output != NULL);
	assert(!fifo_buffer_is_empty(s->output));

	if (s->out_source != NULL)
		return;

	s->out_source = g_io_create_watch(s->channel, G_IO_OUT);
	g_source_set_callback(s->out_source, (GSourceFunc)tcp_event, s, NULL);
	g_source_attach(s->out_source, io_thread_context());
}

static void
tcp_socket_unschedule_write(struct tcp_socket *s)
{
	if (s->out_source == NULL)
		return;

	g_source_destroy(s->out_source);
	g_source_unref(s->out_source);
	s->out_source = NULL;
}

/**
 * Close the socket.  Caller must lock the mutex.
 */
static void
tcp_socket_close(struct tcp_socket *s)
{
	tcp_socket_unschedule_read(s);
	tcp_socket_unschedule_write(s);

	if (s->channel != NULL) {
		g_io_channel_unref(s->channel);
		s->channel = NULL;
	}

	if (s->input != NULL) {
		fifo_buffer_free(s->input);
		s->input = NULL;
	}

	if (s->output != NULL) {
		fifo_buffer_free(s->output);
		s->output = NULL;
	}
}

static gpointer
tcp_socket_close_callback(gpointer data)
{
	struct tcp_socket *s = data;

	g_mutex_lock(s->mutex);
	tcp_socket_close(s);
	g_mutex_unlock(s->mutex);

	return NULL;
}

static void
tcp_socket_close_indirect(struct tcp_socket *s)
{
	io_thread_call(tcp_socket_close_callback, s);

	assert(s->channel == NULL);
	assert(s->in_source == NULL);
	assert(s->out_source == NULL);
}

static void
tcp_handle_input(struct tcp_socket *s)
{
	size_t length;
	const void *p = fifo_buffer_read(s->input, &length);
	if (p == NULL)
		return;

	g_mutex_unlock(s->mutex);
	size_t consumed = s->handler->data(p, length, s->handler_ctx);
	g_mutex_lock(s->mutex);
	if (consumed > 0 && s->input != NULL)
		fifo_buffer_consume(s->input, consumed);
}

static bool
tcp_in_event(struct tcp_socket *s)
{
	assert(s != NULL);
	assert(s->channel != NULL);

	g_mutex_lock(s->mutex);

	size_t max_length;
	void *p = fifo_buffer_write(s->input, &max_length);
	if (p == NULL) {
		GError *error = g_error_new_literal(tcp_socket_quark(), 0,
						    "buffer overflow");
		tcp_socket_close(s);
		g_mutex_unlock(s->mutex);
		s->handler->error(error, s->handler_ctx);
		return false;
	}

	gsize bytes_read;
	GError *error = NULL;
	GIOStatus status = g_io_channel_read_chars(s->channel,
						   p, max_length,
						   &bytes_read, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		fifo_buffer_append(s->input, bytes_read);
		tcp_handle_input(s);
		g_mutex_unlock(s->mutex);
		return true;

	case G_IO_STATUS_AGAIN:
		/* try again later */
		g_mutex_unlock(s->mutex);
		return true;

	case G_IO_STATUS_EOF:
		/* peer disconnected */
		tcp_socket_close(s);
		g_mutex_unlock(s->mutex);
		s->handler->disconnected(s->handler_ctx);
		return false;

	case G_IO_STATUS_ERROR:
		/* I/O error */
		tcp_socket_close(s);
		g_mutex_unlock(s->mutex);
		s->handler->error(error, s->handler_ctx);
		return false;
	}

	/* unreachable */
	assert(false);
	return true;
}

static bool
tcp_out_event(struct tcp_socket *s)
{
	assert(s != NULL);
	assert(s->channel != NULL);

	g_mutex_lock(s->mutex);

	size_t length;
	const void *p = fifo_buffer_read(s->output, &length);
	if (p == NULL) {
		/* no more data in the output buffer, remove the
		   output event */
		tcp_socket_unschedule_write(s);
		g_mutex_unlock(s->mutex);
		return false;
	}

	gsize bytes_written;
	GError *error = NULL;
	GIOStatus status = g_io_channel_write_chars(s->channel, p, length,
						    &bytes_written, &error);
	switch (status) {
	case G_IO_STATUS_NORMAL:
		fifo_buffer_consume(s->output, bytes_written);
		g_mutex_unlock(s->mutex);
		return true;

	case G_IO_STATUS_AGAIN:
		tcp_socket_schedule_write(s);
		g_mutex_unlock(s->mutex);
		return true;

	case G_IO_STATUS_EOF:
		/* peer disconnected */
		tcp_socket_close(s);
		g_mutex_unlock(s->mutex);
		s->handler->disconnected(s->handler_ctx);
		return false;

	case G_IO_STATUS_ERROR:
		/* I/O error */
		tcp_socket_close(s);
		g_mutex_unlock(s->mutex);
		s->handler->error(error, s->handler_ctx);
		return false;
	}

	/* unreachable */
	g_mutex_unlock(s->mutex);
	assert(false);
	return true;
}

static gboolean
tcp_event(G_GNUC_UNUSED GIOChannel *source, GIOCondition condition,
	  gpointer data)
{
	struct tcp_socket *s = data;

	assert(source == s->channel);

	switch (condition) {
	case G_IO_IN:
	case G_IO_PRI:
		return tcp_in_event(s);

	case G_IO_OUT:
		return tcp_out_event(s);

	case G_IO_ERR:
	case G_IO_HUP:
	case G_IO_NVAL:
		tcp_socket_close(s);
		s->handler->disconnected(s->handler_ctx);
		return false;
	}

	/* unreachable */
	assert(false);
	return false;
}

struct tcp_socket *
tcp_socket_new(int fd,
	       const struct tcp_socket_handler *handler, void *ctx)
{
	assert(fd >= 0);
	assert(handler != NULL);
	assert(handler->data != NULL);
	assert(handler->error != NULL);
	assert(handler->disconnected != NULL);

	struct tcp_socket *s = g_new(struct tcp_socket, 1);
	s->handler = handler;
	s->handler_ctx = ctx;
	s->mutex = g_mutex_new();

	g_mutex_lock(s->mutex);

	s->channel = g_io_channel_new_socket(fd);
	/* GLib is responsible for closing the file descriptor */
	g_io_channel_set_close_on_unref(s->channel, true);
	/* NULL encoding means the stream is binary safe */
	g_io_channel_set_encoding(s->channel, NULL, NULL);
	/* no buffering */
	g_io_channel_set_buffered(s->channel, false);

	s->input = fifo_buffer_new(4096);
	s->output = fifo_buffer_new(4096);

	s->in_source = NULL;
	s->out_source = NULL;

	tcp_socket_schedule_read(s);

	g_mutex_unlock(s->mutex);

	return s;
}

void
tcp_socket_free(struct tcp_socket *s)
{
	tcp_socket_close_indirect(s);
	g_mutex_free(s->mutex);
	g_free(s);
}

bool
tcp_socket_send(struct tcp_socket *s, const void *data, size_t length)
{
	assert(s != NULL);

	g_mutex_lock(s->mutex);

	if (s->output == NULL || s->channel == NULL) {
		/* already disconnected */
		g_mutex_unlock(s->mutex);
		return false;
	}

	size_t max_length;
	void *p = fifo_buffer_write(s->output, &max_length);
	if (p == NULL || max_length < length) {
		/* buffer is full */
		g_mutex_unlock(s->mutex);
		return false;
	}

	memcpy(p, data, length);
	fifo_buffer_append(s->output, length);
	tcp_socket_schedule_write(s);

	g_mutex_unlock(s->mutex);
	return true;
}

