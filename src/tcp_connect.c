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

#include "config.h"
#include "tcp_connect.h"
#include "fd_util.h"
#include "io_thread.h"
#include "glib_compat.h"
#include "glib_socket.h"

#include <assert.h>
#include <errno.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

struct tcp_connect {
	const struct tcp_connect_handler *handler;
	void *handler_ctx;

	int fd;
	GSource *source;

	unsigned timeout_ms;
	GSource *timeout_source;
};

static bool
is_in_progress_errno(int e)
{
#ifdef WIN32
	return e == WSAEINPROGRESS || e == WSAEWOULDBLOCK;
#else
	return e == EINPROGRESS;
#endif
}

static gboolean
tcp_connect_event(G_GNUC_UNUSED GIOChannel *source,
		  G_GNUC_UNUSED GIOCondition condition,
		  gpointer data)
{
	struct tcp_connect *c = data;

	assert(c->source != NULL);
	assert(c->timeout_source != NULL);

	/* clear the socket source */
	g_source_unref(c->source);
	c->source = NULL;

	/* delete the timeout source */
	g_source_destroy(c->timeout_source);
	g_source_unref(c->timeout_source);
	c->timeout_source = NULL;

	/* obtain the connect result */
	int s_err = 0;
	socklen_t s_err_size = sizeof(s_err);
	if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR,
		       (char*)&s_err, &s_err_size) < 0)
		s_err = errno;

	if (s_err == 0) {
		/* connection established successfully */

		c->handler->success(c->fd, c->handler_ctx);
	} else {
		/* there was an I/O error; close the socket and pass
		   the error to the handler */

		close_socket(c->fd);

		GError *error =
			g_error_new_literal(g_file_error_quark(), s_err,
					    g_strerror(s_err));
		c->handler->error(error, c->handler_ctx);
	}

	return false;
}

static gboolean
tcp_connect_timeout(gpointer data)
{
	struct tcp_connect *c = data;

	assert(c->source != NULL);
	assert(c->timeout_source != NULL);

	/* clear the timeout source */
	g_source_unref(c->timeout_source);
	c->timeout_source = NULL;

	/* delete the socket source */
	g_source_destroy(c->source);
	g_source_unref(c->source);
	c->source = NULL;

	/* report timeout to handler */
	c->handler->timeout(c->handler_ctx);

	return false;
}

static gpointer
tcp_connect_init(gpointer data)
{
	struct tcp_connect *c = data;

	/* create a connect source */
	GIOChannel *channel = g_io_channel_new_socket(c->fd);
	c->source = g_io_create_watch(channel, G_IO_OUT);
	g_io_channel_unref(channel);

	g_source_set_callback(c->source, (GSourceFunc)tcp_connect_event, c,
			      NULL);
	g_source_attach(c->source, io_thread_context());

	/* create a timeout source */
	if (c->timeout_ms > 0)
		c->timeout_source =
			io_thread_timeout_add(c->timeout_ms,
					      tcp_connect_timeout, c);

	return NULL;
}

void
tcp_connect_address(const struct sockaddr *address, size_t address_length,
		    unsigned timeout_ms,
		    const struct tcp_connect_handler *handler, void *ctx,
		    struct tcp_connect **handle_r)
{
	assert(address != NULL);
	assert(address_length > 0);
	assert(handler != NULL);
	assert(handler->success != NULL);
	assert(handler->error != NULL);
	assert(handler->canceled != NULL);
	assert(handler->timeout != NULL || timeout_ms == 0);
	assert(handle_r != NULL);
	assert(*handle_r == NULL);

	int fd = socket_cloexec_nonblock(address->sa_family, SOCK_STREAM, 0);
	if (fd < 0) {
		GError *error =
			g_error_new_literal(g_file_error_quark(), errno,
					    g_strerror(errno));
		handler->error(error, ctx);
		return;
	}

	int ret = connect(fd, address, address_length);
	if (ret >= 0) {
		/* quick connect, no I/O thread */
		handler->success(fd, ctx);
		return;
	}

	if (!is_in_progress_errno(errno)) {
		GError *error =
			g_error_new_literal(g_file_error_quark(), errno,
					    g_strerror(errno));
		close_socket(fd);
		handler->error(error, ctx);
		return;
	}

	/* got EINPROGRESS, use the I/O thread to wait for the
	   operation to finish */

	struct tcp_connect *c = g_new(struct tcp_connect, 1);
	c->handler = handler;
	c->handler_ctx = ctx;
	c->fd = fd;
	c->source = NULL;
	c->timeout_ms = timeout_ms;
	c->timeout_source = NULL;

	*handle_r = c;

	io_thread_call(tcp_connect_init, c);
}

static gpointer
tcp_connect_cancel_callback(gpointer data)
{
	struct tcp_connect *c = data;

	assert((c->source == NULL) == (c->timeout_source == NULL));

	if (c->source == NULL)
		return NULL;

	/* delete the socket source */
	g_source_destroy(c->source);
	g_source_unref(c->source);
	c->source = NULL;

	/* delete the timeout source */
	g_source_destroy(c->timeout_source);
	g_source_unref(c->timeout_source);
	c->timeout_source = NULL;

	/* close the socket */
	close_socket(c->fd);

	/* notify the handler */
	c->handler->canceled(c->handler_ctx);

	return NULL;
}

void
tcp_connect_cancel(struct tcp_connect *c)
{
	if (c->source == NULL)
		return;

	io_thread_call(tcp_connect_cancel_callback, c);
}

void
tcp_connect_free(struct tcp_connect *c)
{
	assert(c->source == NULL);

	g_free(c);
}
