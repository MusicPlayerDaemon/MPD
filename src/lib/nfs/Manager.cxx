// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Manager.hxx"
#include "Connection.hxx"
#include "Error.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "event/Loop.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/DeleteDisposer.hxx"
#include "util/Domain.hxx"
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

	CoarseTimerEvent disconnect_timer;

public:
	ManagedConnection(NfsManager &_manager, EventLoop &_loop,
			  NfsContext &&_context,
			  std::string_view _server,
			  std::string_view _export_name)
		:NfsConnection(_loop, std::move(_context), _server, _export_name),
		 manager(_manager),
		 disconnect_timer(_loop, BIND_THIS_METHOD(OnDisconnectTimer))
	{
		disconnect_timer.Schedule(std::chrono::minutes{1});
	}

	void Use() noexcept {
		disconnect_timer.Schedule(std::chrono::minutes{1});
	}

protected:
	void OnDisconnectTimer() noexcept {
		if (IsUnused())
			manager.DeleteConnection(*this);
	}

	/* virtual methods from NfsConnection */
	void OnNfsConnectionError(std::exception_ptr e) noexcept override;

	void OnNfsConnectionUnused() noexcept override {
		disconnect_timer.Schedule(std::chrono::minutes{1});
	}
};

void
NfsManager::ManagedConnection::OnNfsConnectionError(std::exception_ptr e) noexcept
{
	FmtError(nfs_domain, "NFS error on '{}:{}': {}",
		 GetServer(), GetExportName(), e);

	manager.DeleteConnection(*this);
}

NfsManager::NfsManager(EventLoop &_loop) noexcept
	:event_loop(_loop) {}

NfsManager::~NfsManager() noexcept
{
	assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

	connections.clear_and_dispose(DeleteDisposer());
}

NfsConnection &
NfsManager::GetConnection(NfsContext &&context, const struct nfs_url &url,
			  bool lookup_existing)
{
	if (lookup_existing) {
		for (auto &c : connections) {
			if (c.GetServer() == url.server &&
			    c.GetExportName() == url.path) {
				c.Use();
				return c;
			}
		}
	}

	auto c = new ManagedConnection(*this, GetEventLoop(),
				       std::move(context),
				       url.server, url.path);
	connections.push_front(*c);
	return *c;
}

inline void
NfsManager::DeleteConnection(ManagedConnection &c) noexcept
{
	assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

	connections.erase_and_dispose(connections.iterator_to(c), DeleteDisposer{});
}
