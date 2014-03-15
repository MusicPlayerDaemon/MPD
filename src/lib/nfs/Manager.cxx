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
#include "Manager.hxx"
#include "event/Loop.hxx"
#include "Log.hxx"

void
NfsManager::ManagedConnection::OnNfsConnectionError(Error &&error)
{
	FormatError(error, "NFS error on %s:%s", GetServer(), GetExportName());

	manager.connections.erase(Key(GetServer(), GetExportName()));
}

NfsConnection &
NfsManager::GetConnection(const char *server, const char *export_name)
{
	assert(server != nullptr);
	assert(export_name != nullptr);
	assert(loop.IsInside());

	const std::string key = Key(server, export_name);

	auto e = connections.emplace(std::piecewise_construct,
				     std::forward_as_tuple(key),
				     std::forward_as_tuple(*this, loop,
							   server,
							   export_name));
	return e.first->second;
}
