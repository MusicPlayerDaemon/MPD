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
#include "EventPipe.hxx"
#include "system/fd_util.h"
#include "system/FatalError.hxx"
#include "Compiler.h"

#include <assert.h>
#include <unistd.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#include <cstring> /* for memset() */
#endif

#ifdef WIN32
static bool PoorSocketPair(int fd[2]);
#endif

EventPipe::EventPipe()
{
#ifdef WIN32
	bool success = PoorSocketPair(fds);
#else
	bool success = pipe_cloexec_nonblock(fds) >= 0;
#endif
	if (!success)
		FatalSystemError("pipe() has failed");
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
 * Due to limited protocol/address family support and primitive error handling
 * it's better to keep this as a private implementation detail of EventPipe
 * rather than wide-available API.
 */
static bool PoorSocketPair(int fd[2])
{
	assert (fd != nullptr);

	SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == INVALID_SOCKET)
		return false;

	sockaddr_in address;
	std::memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	int ret = bind(listen_socket,
		       reinterpret_cast<sockaddr*>(&address),
		       sizeof(address));

	if (ret < 0) {
		SafeCloseSocket(listen_socket);
		return false;
	}

	ret = listen(listen_socket, 1);

	if (ret < 0) {
		SafeCloseSocket(listen_socket);
		return false;
	}

	int address_len = sizeof(address);
	ret = getsockname(listen_socket,
			  reinterpret_cast<sockaddr*>(&address),
			  &address_len);

	if (ret < 0) {
		SafeCloseSocket(listen_socket);
		return false;
	}

	SOCKET socket0 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socket0 == INVALID_SOCKET) {
		SafeCloseSocket(listen_socket);
		return false;
	}

	ret = connect(socket0,
		      reinterpret_cast<sockaddr*>(&address),
		      sizeof(address));

	if (ret < 0) {
		SafeCloseSocket(listen_socket);
		SafeCloseSocket(socket0);
		return false;
	}

	SOCKET socket1 = accept(listen_socket, nullptr, nullptr);
	if (socket1 == INVALID_SOCKET) {
		SafeCloseSocket(listen_socket);
		SafeCloseSocket(socket0);
		return false;
	}

	SafeCloseSocket(listen_socket);

	u_long non_block = 1;
	if (ioctlsocket(socket0, FIONBIO, &non_block) < 0
	    || ioctlsocket(socket1, FIONBIO, &non_block) < 0) {
		SafeCloseSocket(socket0);
		SafeCloseSocket(socket1);
		return false;
	}

	fd[0] = static_cast<int>(socket0);
	fd[1] = static_cast<int>(socket1);

	return true;
}

#endif
