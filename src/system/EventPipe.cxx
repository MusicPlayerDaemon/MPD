/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "EventPipe.hxx"
#include "system/fd_util.h"
#include "system/Error.hxx"
#include "util/ScopeExit.hxx"
#include "Compiler.h"

#include <assert.h>
#include <unistd.h>

#ifdef WIN32
#include "net/IPv4Address.hxx"
#include "net/SocketError.hxx"

#include <winsock2.h>
#endif

#ifdef WIN32
static void PoorSocketPair(int fd[2]);
#endif

EventPipe::EventPipe()
{
#ifdef WIN32
	PoorSocketPair(fds);
#else
	if (pipe_cloexec_nonblock(fds) < 0)
		throw MakeErrno("pipe() has failed");
#endif
}

EventPipe::~EventPipe()
{
#ifdef WIN32
	closesocket(fds[0]);
	closesocket(fds[1]);
#else
	close(fds[0]);
	close(fds[1]);
#endif
}

bool
EventPipe::Read()
{
	assert(fds[0] >= 0);
	assert(fds[1] >= 0);

	char buffer[256];
#ifdef WIN32
	return recv(fds[0], buffer, sizeof(buffer), 0) > 0;
#else
	return read(fds[0], buffer, sizeof(buffer)) > 0;
#endif
}

void
EventPipe::Write()
{
	assert(fds[0] >= 0);
	assert(fds[1] >= 0);

#ifdef WIN32
	send(fds[1], "", 1, 0);
#else
	gcc_unused ssize_t nbytes = write(fds[1], "", 1);
#endif
}

#ifdef WIN32

static void SafeCloseSocket(SOCKET s)
{
	int error = WSAGetLastError();
	closesocket(s);
	WSASetLastError(error);
}

/* Our poor man's socketpair() implementation
 * Due to limited protocol/address family support
 * it's better to keep this as a private implementation detail of EventPipe
 * rather than wide-available API.
 */
static void
PoorSocketPair(int fd[2])
{
	assert (fd != nullptr);

	SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == INVALID_SOCKET)
		throw MakeSocketError("Failed to create socket");

	AtScopeExit(listen_socket) {
		closesocket(listen_socket);
	};

	IPv4Address address(IPv4Address::Loopback(), 0);

	int ret = bind(listen_socket,
		       SocketAddress(address).GetAddress(), sizeof(address));
	if (ret < 0)
		throw MakeSocketError("Failed to create socket");

	ret = listen(listen_socket, 1);
	if (ret < 0)
		throw MakeSocketError("Failed to listen on socket");

	int address_len = sizeof(address);
	ret = getsockname(listen_socket,
			  reinterpret_cast<sockaddr*>(&address),
			  &address_len);
	if (ret < 0)
		throw MakeSocketError("Failed to obtain socket bind address");

	SOCKET socket0 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket0 == INVALID_SOCKET)
		throw MakeSocketError("Failed to create socket");

	ret = connect(socket0,
		      reinterpret_cast<sockaddr*>(&address),
		      sizeof(address));

	if (ret < 0) {
		SafeCloseSocket(socket0);
		throw MakeSocketError("Failed to connect socket");
	}

	SOCKET socket1 = accept(listen_socket, nullptr, nullptr);
	if (socket1 == INVALID_SOCKET) {
		SafeCloseSocket(socket0);
		throw MakeSocketError("Failed to accept connection");
	}

	u_long non_block = 1;
	if (ioctlsocket(socket0, FIONBIO, &non_block) < 0
	    || ioctlsocket(socket1, FIONBIO, &non_block) < 0) {
		SafeCloseSocket(socket0);
		SafeCloseSocket(socket1);
		throw MakeSocketError("Failed to enable non-blocking mode on socket");
	}

	fd[0] = static_cast<int>(socket0);
	fd[1] = static_cast<int>(socket1);
}

#endif
