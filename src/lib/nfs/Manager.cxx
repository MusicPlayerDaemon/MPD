// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
