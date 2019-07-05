/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "EventPipe.hxx"
#include "FileDescriptor.hxx"
#include "system/Error.hxx"
#include "util/Compiler.h"

#include <assert.h>
#include <unistd.h>

#ifdef _WIN32
#include "net/IPv4Address.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketError.hxx"
#endif

#ifdef _WIN32
static void PoorSocketPair(int fd[2]);
#endif

EventPipe::EventPipe()
{
#ifdef _WIN32
	PoorSocketPair(fds);
#else
	FileDescriptor r, w;
	if (!FileDescriptor::CreatePipeNonBlock(r, w))
		throw MakeErrno("pipe() has failed");

	fds[0] = r.Steal();
	fds[1] = w.Steal();
#endif
}

EventPipe::~EventPipe() noexcept
{
#ifdef _WIN32
	closesocket(fds[0]);
	closesocket(fds[1]);
#else
	close(fds[0]);
	close(fds[1]);
#endif
}

bool
EventPipe::Read() noexcept
{
	assert(fds[0] >= 0);
	assert(fds[1] >= 0);

	char buffer[256];
#ifdef _WIN32
	return recv(fds[0], buffer, sizeof(buffer), 0) > 0;
#else
	return read(fds[0], buffer, sizeof(buffer)) > 0;
#endif
}

void
EventPipe::Write() noexcept
{
	assert(fds[0] >= 0);
	assert(fds[1] >= 0);

#ifdef _WIN32
	send(fds[1], "", 1, 0);
#else
	gcc_unused ssize_t nbytes = write(fds[1], "", 1);
#endif
}

#ifdef _WIN32

/* Our poor man's socketpair() implementation
 * Due to limited protocol/address family support
 * it's better to keep this as a private implementation detail of EventPipe
 * rather than wide-available API.
 */
static void
PoorSocketPair(int fd[2])
{
	assert (fd != nullptr);

	UniqueSocketDescriptor listen_socket;
	if (!listen_socket.Create(AF_INET, SOCK_STREAM, IPPROTO_TCP))
		throw MakeSocketError("Failed to create socket");

	if (!listen_socket.Bind(IPv4Address(IPv4Address::Loopback(), 0)))
		throw MakeSocketError("Failed to create socket");

	if (!listen_socket.Listen(1))
		throw MakeSocketError("Failed to listen on socket");

	UniqueSocketDescriptor socket0;
	if (!socket0.Create(AF_INET, SOCK_STREAM, IPPROTO_TCP))
		throw MakeSocketError("Failed to create socket");

	if (!socket0.Connect(listen_socket.GetLocalAddress()))
		throw MakeSocketError("Failed to connect socket");

	socket0.SetNonBlocking();

	auto socket1 = listen_socket.AcceptNonBlock();
	if (!socket1.IsDefined())
		throw MakeSocketError("Failed to accept connection");

	fd[0] = socket0.Steal();
	fd[1] = socket1.Steal();
}

#endif
