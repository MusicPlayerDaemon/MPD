// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Manager.hxx"
#include "Connection.hxx"
#include "Error.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "event/Loop.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "Log.hxx"

extern "C" {
#include <nfsc/libnfs.h>
}

static constexpr Domain nfs_domain("nfs");

class NfsManager::ManagedConnection final
	: public NfsConnection,
	  public IntrusiveListHook<>
{
	NfsManager &manager;

public:
	ManagedConnection(NfsManager &_manager, EventLoop &_loop,
			  struct nfs_context *_context,
			  std::string_view _server,
			  std::string_view _export_name)
		:NfsConnection(_loop, _context, _server, _export_name),
		 manager(_manager) {}

protected:
	/* virtual methods from NfsConnection */
	void OnNfsConnectionError(std::exception_ptr e) noexcept override;
};

void
NfsManager::ManagedConnection::OnNfsConnectionError(std::exception_ptr e) noexcept
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
NfsManager::MakeConnection(const char *url)
{
	struct nfs_context *const context = nfs_init_context();
	if (context == nullptr)
		throw std::runtime_error{"nfs_init_context() failed"};

	auto *pu = nfs_parse_url_dir(context, url);
	if (pu == nullptr) {
		AtScopeExit(context) { nfs_destroy_context(context); };
		throw NfsClientError(context, "nfs_parse_url_dir() failed");
	}

	AtScopeExit(pu) { nfs_destroy_url(pu); };

	auto c = new ManagedConnection(*this, GetEventLoop(),
				       context,
				       pu->server, pu->path);
	connections.push_front(*c);
	return *c;
}

NfsConnection &
NfsManager::GetConnection(std::string_view server, std::string_view export_name)
{
	assert(GetEventLoop().IsInside());

	for (auto &c : connections)
		if (c.GetServer() == server &&
		    c.GetExportName() == export_name)
			return c;

	struct nfs_context *const context = nfs_init_context();
	if (context == nullptr)
		throw std::runtime_error{"nfs_init_context() failed"};

	auto c = new ManagedConnection(*this, GetEventLoop(),
				       context,
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
