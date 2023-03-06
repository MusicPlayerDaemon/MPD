// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
