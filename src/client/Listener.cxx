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
