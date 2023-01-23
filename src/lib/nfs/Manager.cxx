/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#include "Manager.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "event/Loop.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/Domain.hxx"
#include "util/StringAPI.hxx"
#include "Log.hxx"

static constexpr Domain nfs_domain("nfs");

void
NfsManager::ManagedConnection::OnNfsConnectionError(std::exception_ptr &&e) noexcept
{
	FmtError(nfs_domain, "NFS error on '{}:{}': {}",
		 GetServer(), GetExportName(), e);

	/* defer deletion so the caller
	   (i.e. NfsConnection::OnSocketReady()) can still use this
	   object */
	manager.ScheduleDelete(*this);
}

NfsManager::~NfsManager() noexcept
{
	assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

	CollectGarbage();

	connections.clear_and_dispose(DeleteDisposer());
}

NfsConnection &
NfsManager::GetConnection(const char *server, const char *export_name) noexcept
{
	assert(server != nullptr);
	assert(export_name != nullptr);
	assert(GetEventLoop().IsInside());

	for (auto &c : connections)
		if (StringIsEqual(server, c.GetServer()) &&
		    StringIsEqual(export_name, c.GetExportName()))
			return c;

	auto c = new ManagedConnection(*this, GetEventLoop(),
				       server, export_name);
	connections.push_front(*c);
	return *c;
}

void
NfsManager::CollectGarbage() noexcept
{
	assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

	garbage.clear_and_dispose(DeleteDisposer());
}

void
NfsManager::OnIdle() noexcept
{
	CollectGarbage();
}
