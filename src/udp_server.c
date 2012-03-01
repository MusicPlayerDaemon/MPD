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

#include "udp_server.h"
#include "io_thread.h"
#include "glib_socket.h"
#include "gcc.h"

#include <glib.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#if GCC_CHECK_VERSION(4, 2)
/* allow C99 initialisers on struct sockaddr_in, even if the
   (non-portable) attribute "sin_zero" is missing */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

struct udp_server {
	const struct udp_server_handler *handler;
	void *handler_ctx;

	int fd;
	GIOChannel *channel;
	GSource *source;

	char buffer[8192];
};

static gboolean
udp_in_event(G_GNUC_UNUSED GIOChannel *source,
	     G_GNUC_UNUSED GIOCondition condition,
	     gpointer data)
{
	struct udp_server *udp = data;

	struct sockaddr_storage address_storage;
	struct sockaddr *address = (struct sockaddr *)&address_storage;
	socklen_t address_length = sizeof(address_storage);

	ssize_t nbytes = recvfrom(udp->fd, udp->buffer, sizeof(udp->buffer),
#ifdef WIN32
				  0,
#else
				  MSG_DONTWAIT,
#endif
				  address, &address_length);
	if (nbytes <= 0)
		return true;

	udp->handler->datagram(udp->fd, udp->buffer, nbytes,
			       address, address_length, udp->handler_ctx);
	return true;
}

struct udp_server *
udp_server_new(unsigned port,
	       const struct udp_server_handler *handler, void *ctx,
	       GError **error_r)
{
	int fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		g_set_error(error_r, udp_server_quark(), errno,
			    "failed to create UDP socket: %s",
			    g_strerror(errno));
		return NULL;
	}

	const struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_addr = {
			.s_addr = htonl(INADDR_ANY),
		},
		.sin_port = htons(port),
#if defined(__linux__) && !GCC_CHECK_VERSION(4, 2)
		.sin_zero = { 0 },
#endif
	};

	if (bind(fd, (const struct sockaddr *)&address, sizeof(address)) < 0) {
		g_set_error(error_r, udp_server_quark(), errno,
			    "failed to bind UDP port %u: %s",
			    port, g_strerror(errno));
		close(fd);
		return NULL;
	}

	struct udp_server *udp = g_new(struct udp_server, 1);
	udp->handler = handler;
	udp->handler_ctx = ctx;

	udp->fd = fd;
	udp->channel = g_io_channel_new_socket(fd);
	/* NULL encoding means the stream is binary safe */
	g_io_channel_set_encoding(udp->channel, NULL, NULL);
	/* no buffering */
	g_io_channel_set_buffered(udp->channel, false);

	udp->source = g_io_create_watch(udp->channel, G_IO_IN);
	g_source_set_callback(udp->source, (GSourceFunc)udp_in_event, udp,
			      NULL);
	g_source_attach(udp->source, io_thread_context());

	return udp;
}

void
udp_server_free(struct udp_server *udp)
{
	g_source_destroy(udp->source);
	g_source_unref(udp->source);
	g_io_channel_unref(udp->channel);
	close(udp->fd);
	g_free(udp);
}
