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

#ifndef MPD_TCP_SOCKET_H
#define MPD_TCP_SOCKET_H

#include <glib.h>

#include <stdbool.h>
#include <stddef.h>

struct sockaddr;

struct tcp_socket_handler {
	/**
	 * New data has arrived.
	 *
	 * @return the number of bytes consumed; 0 if more data is
	 * needed
	 */
	size_t (*data)(const void *data, size_t length, void *ctx);

	void (*error)(GError *error, void *ctx);

	void (*disconnected)(void *ctx);
};

static inline GQuark
tcp_socket_quark(void)
{
	return g_quark_from_static_string("tcp_socket");
}

G_GNUC_MALLOC
struct tcp_socket *
tcp_socket_new(int fd,
	       const struct tcp_socket_handler *handler, void *ctx);

void
tcp_socket_free(struct tcp_socket *s);

bool
tcp_socket_send(struct tcp_socket *s, const void *data, size_t length);

#endif
