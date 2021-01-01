/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

/*
 * This library provides easy helper functions for working with
 * sockets.
 *
 */

#ifndef MPD_SOCKET_UTIL_HXX
#define MPD_SOCKET_UTIL_HXX

class UniqueSocketDescriptor;
class SocketAddress;

/**
 * Creates a socket listening on the specified address.  This is a
 * shortcut for socket(), bind() and listen().
 * When a local socket is created (domain == AF_LOCAL), its
 * permissions will be stripped down to prevent unauthorized
 * access. The caller is responsible to apply proper permissions
 * at a later point.
 *
 * Throws on error.
 *
 * @param domain the socket domain, e.g. PF_INET6
 * @param type the socket type, e.g. SOCK_STREAM
 * @param protocol the protocol, usually 0 to let the kernel choose
 * @param address the address to listen on
 * @param backlog the backlog parameter for the listen() system call
 * @return the socket file descriptor
 */
UniqueSocketDescriptor
socket_bind_listen(int domain, int type, int protocol,
		   SocketAddress address,
		   int backlog);

#endif
