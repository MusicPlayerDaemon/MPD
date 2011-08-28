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

#ifndef MPD_TCP_CONNECT_H
#define MPD_TCP_CONNECT_H

#include <glib.h>

struct sockaddr;

struct tcp_connect_handler {
	/**
	 * The connection was established successfully.
	 *
	 * @param fd a file descriptor that must be closed with
	 * close_socket() when finished
	 */
	void (*success)(int fd, void *ctx);

	/**
	 * An error has occurred.  The method is responsible for
	 * freeing the GError.
	 */
	void (*error)(GError *error, void *ctx);

	/**
	 * The connection could not be established in the specified
	 * time span.
	 */
	void (*timeout)(void *ctx);

	/**
	 * The operation was canceled before a result was available.
	 */
	void (*canceled)(void *ctx);
};

struct tcp_connect;

/**
 * Establish a TCP connection to the specified address.
 *
 * Note that the result may be available before this function returns.
 *
 * The caller must free this object with tcp_connect_free().
 *
 * @param timeout_ms time out after this number of milliseconds; 0
 * means no timeout
 * @param handle_r a handle that can be used to cancel the operation;
 * the caller must initialize it to NULL
 */
void
tcp_connect_address(const struct sockaddr *address, size_t address_length,
		    unsigned timeout_ms,
		    const struct tcp_connect_handler *handler, void *ctx,
		    struct tcp_connect **handle_r);

/**
 * Cancel the operation.  It is possible that the result is delivered
 * before the operation has been canceled; in that case, the
 * canceled() handler method will not be invoked.
 *
 * Even after calling this function, tcp_connect_free() must still be
 * called to free memory.
 */
void
tcp_connect_cancel(struct tcp_connect *handle);

/**
 * Free memory used by this object.
 *
 * This function is not thread safe.  It must not be called while
 * other threads are still working with it.  If no callback has been
 * invoked so far, then you must call tcp_connect_cancel() to release
 * I/O thread resources, before calling this function.
 */
void
tcp_connect_free(struct tcp_connect *handle);

#endif
