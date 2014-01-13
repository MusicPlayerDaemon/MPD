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

#ifndef MPD_SERVER_SOCKET_HXX
#define MPD_SERVER_SOCKET_HXX

#include <list>

#include <stddef.h>

struct sockaddr;
class EventLoop;
class Error;
class AllocatedPath;

typedef void (*server_socket_callback_t)(int fd,
					 const struct sockaddr *address,
					 size_t address_length, int uid,
					 void *ctx);

class OneServerSocket;

/**
 * A socket that accepts incoming stream connections (e.g. TCP).
 */
class ServerSocket {
	friend class OneServerSocket;

	EventLoop &loop;

	std::list<OneServerSocket> sockets;

	unsigned next_serial;

public:
	ServerSocket(EventLoop &_loop);
	~ServerSocket();

	EventLoop &GetEventLoop() {
		return loop;
	}

private:
	OneServerSocket &AddAddress(const sockaddr &address, size_t length);

	/**
	 * Add a listener on a port on all IPv4 interfaces.
	 *
	 * @param port the TCP port
	 */
	void AddPortIPv4(unsigned port);

	/**
	 * Add a listener on a port on all IPv6 interfaces.
	 *
	 * @param port the TCP port
	 */
	void AddPortIPv6(unsigned port);

public:
	/**
	 * Add a listener on a port on all interfaces.
	 *
	 * @param port the TCP port
	 * @param error_r location to store the error occurring, or nullptr to
	 * ignore errors
	 * @return true on success
	 */
	bool AddPort(unsigned port, Error &error);

	/**
	 * Resolves a host name, and adds listeners on all addresses in the
	 * result set.
	 *
	 * @param hostname the host name to be resolved
	 * @param port the TCP port
	 * @param error_r location to store the error occurring, or nullptr to
	 * ignore errors
	 * @return true on success
	 */
	bool AddHost(const char *hostname, unsigned port, Error &error);

	/**
	 * Add a listener on a Unix domain socket.
	 *
	 * @param path the absolute socket path
	 * @param error_r location to store the error occurring, or nullptr to
	 * ignore errors
	 * @return true on success
	 */
	bool AddPath(AllocatedPath &&path, Error &error);

	/**
	 * Add a socket descriptor that is accepting connections.  After this
	 * has been called, don't call server_socket_open(), because the
	 * socket is already open.
	 */
	bool AddFD(int fd, Error &error);

	bool Open(Error &error);
	void Close();

protected:
	virtual void OnAccept(int fd, const sockaddr &address,
			      size_t address_length, int uid) = 0;
};

#endif
