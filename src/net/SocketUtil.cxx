/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "SocketUtil.hxx"
#include "SocketAddress.hxx"
#include "SocketError.hxx"
#include "system/fd_util.h"

int
socket_bind_listen(int domain, int type, int protocol,
		   SocketAddress address,
		   int backlog)
{
	int fd, ret;
	const int reuse = 1;

	fd = socket_cloexec_nonblock(domain, type, protocol);
	if (fd < 0)
		throw MakeSocketError("Failed to create socket");

	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			 (const char *) &reuse, sizeof(reuse));
	if (ret < 0) {
		auto error = GetSocketError();
		close_socket(fd);
		throw MakeSocketError(error, "setsockopt() failed");
	}

	ret = bind(fd, address.GetAddress(), address.GetSize());
	if (ret < 0) {
		auto error = GetSocketError();
		close_socket(fd);
		throw MakeSocketError(error, "Failed to bind socket");
	}

	ret = listen(fd, backlog);
	if (ret < 0) {
		auto error = GetSocketError();
		close_socket(fd);
		throw MakeSocketError(error, "Failed to listen on socket");
	}

#if defined(HAVE_STRUCT_UCRED) && defined(SO_PASSCRED)
	setsockopt(fd, SOL_SOCKET, SO_PASSCRED,
		   (const char *) &reuse, sizeof(reuse));
#endif

	return fd;
}

int
socket_keepalive(int fd)
{
	const int reuse = 1;

	return setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
			  (const char *)&reuse, sizeof(reuse));
}
