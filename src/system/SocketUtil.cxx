/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "SocketError.hxx"
#include "fd_util.h"

#include <unistd.h>

#ifndef WIN32
#include <sys/socket.h>
#else
#include <ws2tcpip.h>
#include <winsock.h>
#endif

#ifdef HAVE_IPV6
#include <string.h>
#endif

int
socket_bind_listen(int domain, int type, int protocol,
		   const struct sockaddr *address, size_t address_length,
		   int backlog,
		   Error &error)
{
	int fd, ret;
	const int reuse = 1;

	fd = socket_cloexec_nonblock(domain, type, protocol);
	if (fd < 0) {
		SetSocketError(error);
		error.AddPrefix("Failed to create socket: ");
		return -1;
	}

	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
			 (const char *) &reuse, sizeof(reuse));
	if (ret < 0) {
		SetSocketError(error);
		error.AddPrefix("setsockopt() failed: ");
		close_socket(fd);
		return -1;
	}

	ret = bind(fd, address, address_length);
	if (ret < 0) {
		SetSocketError(error);
		close_socket(fd);
		return -1;
	}

	ret = listen(fd, backlog);
	if (ret < 0) {
		SetSocketError(error);
		error.AddPrefix("listen() failed: ");
		close_socket(fd);
		return -1;
	}

#ifdef HAVE_STRUCT_UCRED
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
