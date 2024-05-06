// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Manager.hxx"
#include "Connection.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "event/Loop.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/Domain.hxx"
#include "util/StringAPI.hxx"
#include "Log.hxx"

static constexpr Domain nfs_domain("nfs");

class NfsManager::ManagedConnection final
	: public NfsConnection,
	  public IntrusiveListHook<>
{
	NfsManager &manager;

public:
	ManagedConnection(NfsManager &_manager, EventLoop &_loop,
			  std::string_view _server,
			  std::string_view _export_name) noexcept
		:NfsConnection(_loop, _server, _export_name),
		 manager(_manager) {}

protected:
	/* virtual methods from NfsConnection */
	void OnNfsConnectionError(std::exception_ptr &&e) noexcept override;
};

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

NfsManager::NfsManager(EventLoop &_loop) noexcept
	:idle_event(_loop, BIND_THIS_METHOD(OnIdle)) {}

NfsManager::~NfsManager() noexcept
{
	assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

	CollectGarbage();

	connections.clear_and_dispose(DeleteDisposer());
}

NfsConnection &
NfsManager::GetConnection(std::string_view server, std::string_view export_name) noexcept
{
	assert(GetEventLoop().IsInside());

	for (auto &c : connections)
		if (c.GetServer() == server &&
		    c.GetExportName() == export_name)
			return c;

	auto c = new ManagedConnection(*this, GetEventLoop(),
				       server, export_name);
	connections.push_front(*c);
	return *c;
}

inline void
NfsManager::ScheduleDelete(ManagedConnection &c) noexcept
{
	connections.erase(connections.iterator_to(c));
	garbage.push_front(c);
	idle_event.Schedule();
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
