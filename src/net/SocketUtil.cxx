// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "SocketUtil.hxx"
#include "SocketAddress.hxx"
#include "SocketError.hxx"
#include "UniqueSocketDescriptor.hxx"

#include <sys/stat.h>

UniqueSocketDescriptor
socket_bind_listen(int domain, int type, int protocol,
		   SocketAddress address,
		   int backlog)
{
	UniqueSocketDescriptor fd;
	if (!fd.CreateNonBlock(domain, type, protocol))
		throw MakeSocketError("Failed to create socket");

#ifdef HAVE_UN
	if (domain == AF_LOCAL) {
		/* Prevent access until right permissions are set */
		fchmod(fd.Get(), 0);
	}
#endif

	if (!fd.SetReuseAddress())
		throw MakeSocketError("setsockopt() failed");

	if (!fd.Bind(address))
		throw MakeSocketError("Failed to bind socket");

	if (!fd.Listen(backlog))
		throw MakeSocketError("Failed to listen on socket");

#if defined(HAVE_STRUCT_UCRED) && defined(SO_PASSCRED)
	fd.SetBoolOption(SOL_SOCKET, SO_PASSCRED, true);
#endif

	return fd;
}
