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

#ifndef MPD_SERVER_SOCKET_H
#define MPD_SERVER_SOCKET_H

#include <stdbool.h>

#include <glib.h>

struct sockaddr;

typedef void (*server_socket_callback_t)(int fd,
					 const struct sockaddr *address,
					 size_t address_length, int uid,
					 void *ctx);

struct server_socket *
server_socket_new(server_socket_callback_t callback, void *callback_ctx);

void
server_socket_free(struct server_socket *ss);

bool
server_socket_open(struct server_socket *ss, GError **error_r);

void
server_socket_close(struct server_socket *ss);

/**
 * Add a socket descriptor that is accepting connections.  After this
 * has been called, don't call server_socket_open(), because the
 * socket is already open.
 */
bool
server_socket_add_fd(struct server_socket *ss, int fd, GError **error_r);

/**
 * Add a listener on a port on all interfaces.
 *
 * @param port the TCP port
 * @param error_r location to store the error occurring, or NULL to
 * ignore errors
 * @return true on success
 */
bool
server_socket_add_port(struct server_socket *ss, unsigned port,
		       GError **error_r);

/**
 * Resolves a host name, and adds listeners on all addresses in the
 * result set.
 *
 * @param hostname the host name to be resolved
 * @param port the TCP port
 * @param error_r location to store the error occurring, or NULL to
 * ignore errors
 * @return true on success
 */
bool
server_socket_add_host(struct server_socket *ss, const char *hostname,
		       unsigned port, GError **error_r);

/**
 * Add a listener on a Unix domain socket.
 *
 * @param path the absolute socket path
 * @param error_r location to store the error occurring, or NULL to
 * ignore errors
 * @return true on success
 */
bool
server_socket_add_path(struct server_socket *ss, const char *path,
		       GError **error_r);

#endif
