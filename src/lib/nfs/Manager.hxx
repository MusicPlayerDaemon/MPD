// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "event/IdleEvent.hxx"
#include "util/IntrusiveList.hxx"

#include <string_view>

struct nfs_url;
class NfsContext;
class NfsConnection;

/**
 * A manager for NFS connections.  Handles multiple connections to
 * multiple NFS servers.
 */
class NfsManager final {
	class ManagedConnection;
	using List = IntrusiveList<ManagedConnection>;

	List connections;

	IdleEvent idle_event;

public:
	explicit NfsManager(EventLoop &_loop) noexcept;

	/**
	 * Must be run from EventLoop's thread.
	 */
	~NfsManager() noexcept;

	auto &GetEventLoop() const noexcept {
		return idle_event.GetEventLoop();
	}

	/**
	 * Create a new #NfsConnection (or look up an existing one if
	 * #lookup_existing is true).
	 *
	 * Throws on error.
	 */
	[[nodiscard]]
	NfsConnection &GetConnection(NfsContext &&context, const struct nfs_url &url,
				     bool lookup_existing);

private:
	void DeleteConnection(ManagedConnection &c) noexcept;

	/* virtual methods from IdleMonitor */
	void OnIdle() noexcept;
};
