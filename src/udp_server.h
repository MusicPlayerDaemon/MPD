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

#ifndef MPD_UDP_SERVER_H
#define MPD_UDP_SERVER_H

#include <glib.h>

#include <stddef.h>

struct sockaddr;

struct udp_server_handler {
	/**
	 * A datagram was received.
	 */
	void (*datagram)(int fd, const void *data, size_t length,
			 const struct sockaddr *source_address,
			 size_t source_address_length, void *ctx);
};

static inline GQuark
udp_server_quark(void)
{
	return g_quark_from_static_string("udp_server");
}

struct udp_server *
udp_server_new(unsigned port,
	       const struct udp_server_handler *handler, void *ctx,
	       GError **error_r);

void
udp_server_free(struct udp_server *udp);

#endif
