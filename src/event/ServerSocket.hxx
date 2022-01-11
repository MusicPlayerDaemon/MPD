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

#ifndef MPD_SERVER_SOCKET_HXX
#define MPD_SERVER_SOCKET_HXX

#include "config.h"

#include <cassert>
#include <list>

class SocketAddress;
class AllocatedSocketAddress;
class UniqueSocketDescriptor;
class EventLoop;
class AllocatedPath;

/**
 * A socket that accepts incoming stream connections (e.g. TCP).
 */
class ServerSocket {
	class OneServerSocket;

	EventLoop &loop;

	std::list<OneServerSocket> sockets;

#ifdef HAVE_TCP
	/**
	 * A non-negative value sets the IPPROTO_IP/IP_TOS or
	 * IPPROTO_IPV6/IPV6_TCLASS socket option.
	 */
	int dscp_class = -1;
#endif

	unsigned next_serial = 1;

public:
	ServerSocket(EventLoop &_loop) noexcept;
	~ServerSocket() noexcept;

	EventLoop &GetEventLoop() const noexcept {
		return loop;
	}

#ifdef HAVE_TCP
	void SetDscpClass(int _dscp_class) noexcept {
		assert(sockets.empty());

		dscp_class = _dscp_class;
	}
#endif

private:
	template<typename A>
	OneServerSocket &AddAddress(A &&address) noexcept;

	/**
	 * Add a listener on a port on all IPv4 interfaces.
	 *
	 * @param port the TCP port
	 */
	void AddPortIPv4(unsigned port) noexcept;

	/**
	 * Add a listener on a port on all IPv6 interfaces.
	 *
	 * @param port the TCP port
	 */
	void AddPortIPv6(unsigned port) noexcept;

public:
	/**
	 * Add a listener on a port on all interfaces.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @param port the TCP port
	 */
	void AddPort(unsigned port);

	/**
	 * Resolves a host name, and adds listeners on all addresses in the
	 * result set.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @param hostname the host name to be resolved
	 * @param port the TCP port
	 */
	void AddHost(const char *hostname, unsigned port);

	/**
	 * Add a listener on a local socket.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @param path the absolute socket path
	 */
	void AddPath(AllocatedPath &&path);

	/**
	 * Add a listener on an abstract local socket (Linux specific).
	 *
	 * Throws on error.
	 *
	 * @param name the abstract socket name, starting with a '@'
	 * instead of a null byte
	 */
	void AddAbstract(const char *name);

	/**
	 * Add a socket descriptor that is accepting connections.  After this
	 * has been called, don't call server_socket_open(), because the
	 * socket is already open.
	 *
	 * Throws #std::runtime_error on error.
	 */
	void AddFD(UniqueSocketDescriptor fd);

	void AddFD(UniqueSocketDescriptor fd,
		   AllocatedSocketAddress &&address) noexcept;

	bool IsEmpty() const noexcept {
		return sockets.empty();
	}

	/**
	 * Throws #std::runtime_error on error.
	 */
	void Open();

	void Close() noexcept;

protected:
	virtual void OnAccept(UniqueSocketDescriptor fd,
			      SocketAddress address, int uid) noexcept = 0;
};

#endif
