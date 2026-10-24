// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Manager.hxx"
#include "Connection.hxx"
#include "Error.hxx"
#include "Url.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "event/Loop.hxx"
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

	/**
	 * Was this connection created by MakeConnection()?  These are
	 * deleted as soon as they are unused, because each call to
	 * MakeConnection() creates a new one.
	 */
	const bool transient;

public:
	ManagedConnection(NfsManager &_manager, EventLoop &_loop,
			  NfsContext &&_context,
			  std::string_view _server,
			  std::string_view _export_name,
			  bool _transient)
		:NfsConnection(_loop, std::move(_context), _server, _export_name),
		 manager(_manager), transient(_transient) {}

	[[gnu::pure]]
	bool IsObsolete() const noexcept {
		return transient && IsUnused();
	}

protected:
	/* virtual methods from NfsConnection */
	void OnNfsConnectionError(std::exception_ptr e) noexcept override;

	void OnNfsConnectionUnused() noexcept override {
		if (transient)
			/* defer deletion to NfsManager::OnIdle() */
			manager.idle_event.Schedule();
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
	:idle_event(_loop, BIND_THIS_METHOD(OnIdle)) {}

NfsManager::~NfsManager() noexcept
{
	assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

	connections.clear_and_dispose(DeleteDisposer());
}

NfsConnection &
NfsManager::GetConnection(NfsContext &&context, const struct nfs_url &url,
			  bool lookup_existing)
{
	if (lookup_existing)
		for (auto &c : connections)
			if (c.GetServer() == url.server &&
			    c.GetExportName() == url.path)
				return c;

	auto c = new ManagedConnection(*this, GetEventLoop(),
				       std::move(context),
				       url.server, url.path,
				       !lookup_existing);
	connections.push_front(*c);
	return *c;
}

inline void
NfsManager::DeleteConnection(ManagedConnection &c) noexcept
{
	assert(!GetEventLoop().IsAlive() || GetEventLoop().IsInside());

	connections.erase_and_dispose(connections.iterator_to(c), DeleteDisposer{});
}

inline void
NfsManager::OnIdle() noexcept
{
	connections.remove_and_dispose_if([](const ManagedConnection &c){
		return c.IsObsolete();
	}, DeleteDisposer());
}
