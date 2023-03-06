// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Listener.hxx"
#include "Client.hxx"
#include "Permission.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "net/SocketAddress.hxx"
#include "config.h"

static unsigned
GetPermissions(SocketAddress address, int uid) noexcept
{
	(void)uid; // TODO: implement option to derive permissions from uid

#ifdef HAVE_UN
	if (address.GetFamily() == AF_LOCAL)
		return GetLocalPermissions();
#endif

#ifdef HAVE_TCP
	if (int permissions = GetPermissionsFromAddress(address);
	    permissions >= 0)
		return permissions;
#endif

	return getDefaultPermissions();
}

void
ClientListener::OnAccept(UniqueSocketDescriptor fd,
			 SocketAddress address, int uid) noexcept
{

	client_new(GetEventLoop(), partition,
		   std::move(fd), address, uid,
		   GetPermissions(address, uid));
}
