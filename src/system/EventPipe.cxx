// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "EventPipe.hxx"
#include "io/FileDescriptor.hxx"
#include "system/Error.hxx"

#include <cassert>

#ifdef _WIN32
#include "net/IPv4Address.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketError.hxx"
#endif

#ifdef _WIN32
static void PoorSocketPair(UniqueSocketDescriptor &socket0,
			   UniqueSocketDescriptor &socket01);
#endif

EventPipe::EventPipe()
{
#ifdef _WIN32
	PoorSocketPair(r, w);
#else
	if (!UniqueFileDescriptor::CreatePipeNonBlock(r, w))
		throw MakeErrno("pipe() has failed");
#endif
}

EventPipe::~EventPipe() noexcept = default;

bool
EventPipe::Read() noexcept
{
	assert(r.IsDefined());
	assert(w.IsDefined());

	char buffer[256];
	return r.Read(buffer, sizeof(buffer)) > 0;
}

void
EventPipe::Write() noexcept
{
	assert(r.IsDefined());
	assert(w.IsDefined());

	w.Write("", 1);
}

#ifdef _WIN32

/* Our poor man's socketpair() implementation
 * Due to limited protocol/address family support
 * it's better to keep this as a private implementation detail of EventPipe
 * rather than wide-available API.
 */
static void
PoorSocketPair(UniqueSocketDescriptor &socket0, UniqueSocketDescriptor &socket1)
{
	UniqueSocketDescriptor listen_socket;
	if (!listen_socket.Create(AF_INET, SOCK_STREAM, IPPROTO_TCP))
		throw MakeSocketError("Failed to create socket");

	if (!listen_socket.Bind(IPv4Address(IPv4Address::Loopback(), 0)))
		throw MakeSocketError("Failed to create socket");

	if (!listen_socket.Listen(1))
		throw MakeSocketError("Failed to listen on socket");

	if (!socket0.Create(AF_INET, SOCK_STREAM, IPPROTO_TCP))
		throw MakeSocketError("Failed to create socket");

	if (!socket0.Connect(listen_socket.GetLocalAddress()))
		throw MakeSocketError("Failed to connect socket");

	socket0.SetNonBlocking();

	socket1 = listen_socket.AcceptNonBlock();
	if (!socket1.IsDefined())
		throw MakeSocketError("Failed to accept connection");
}

#endif
