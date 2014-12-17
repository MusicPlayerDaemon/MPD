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

#include <string.h>

void
NfsManager::ManagedConnection::OnNfsConnectionError(Error &&error)
{
	FormatError(error, "NFS error on %s:%s", GetServer(), GetExportName());

	/* defer deletion so the caller
	   (i.e. NfsConnection::OnSocketReady()) can still use this
	   object */
	manager.ScheduleDelete(*this);
}

inline bool
NfsManager::Compare::operator()(const LookupKey a,
				const ManagedConnection &b) const
{
	int result = strcmp(a.server, b.GetServer());
	if (result != 0)
		return result < 0;

	result = strcmp(a.export_name, b.GetExportName());
	return result < 0;
}

inline bool
NfsManager::Compare::operator()(const ManagedConnection &a,
				const LookupKey b) const
{
	int result = strcmp(a.GetServer(), b.server);
	if (result != 0)
		return result < 0;

	result = strcmp(a.GetExportName(), b.export_name);
	return result < 0;
}

NfsManager::~NfsManager()
{
	assert(GetEventLoop().IsInside());

	CollectGarbage();

	connections.clear_and_dispose([](ManagedConnection *c){
			delete c;
		});
}

NfsConnection &
NfsManager::GetConnection(const char *server, const char *export_name)
{
	assert(server != nullptr);
	assert(export_name != nullptr);
	assert(GetEventLoop().IsInside());

	Map::insert_commit_data hint;
	auto result = connections.insert_check(LookupKey{server, export_name},
					       Compare(), hint);
	if (result.second) {
		auto c = new ManagedConnection(*this, GetEventLoop(),
					       server, export_name);
		connections.insert_commit(*c, hint);
		return *c;
	} else {
		return *result.first;
	}
}

void
NfsManager::CollectGarbage()
{
	assert(GetEventLoop().IsInside());

	garbage.clear_and_dispose([](ManagedConnection *c){
			delete c;
		});
}

void
NfsManager::OnIdle()
{
	CollectGarbage();
}
