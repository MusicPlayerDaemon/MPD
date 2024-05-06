// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "event/IdleEvent.hxx"
#include "util/IntrusiveList.hxx"

#include <string_view>

class NfsConnection;

/**
 * A manager for NFS connections.  Handles multiple connections to
 * multiple NFS servers.
 */
class NfsManager final {
	class ManagedConnection;
	using List = IntrusiveList<ManagedConnection>;

	List connections;

	/**
	 * A list of "garbage" connection objects.  Their destruction
	 * is postponed because they were thrown into the garbage list
	 * when callers on the stack were still using them.
	 */
	List garbage;

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
	 * Create a new #NfsConnection, parsing the specified "nfs://"
	 * URL.
	 *
	 * Throws on error.
	 */
	[[nodiscard]]
	NfsConnection &MakeConnection(const char *url);

	/**
	 * Look up an existing #NfsConnection (or create a new one if
	 * none matching the given parameters exists).  Unlike
	 * MakeConnection(), this does not support options in a query
	 * string.
	 *
	 * Throws on error.
	 */
	[[nodiscard]]
	NfsConnection &GetConnection(std::string_view server,
				     std::string_view export_name);

private:
	void ScheduleDelete(ManagedConnection &c) noexcept;

	/**
	 * Delete all connections on the #garbage list.
	 */
	void CollectGarbage() noexcept;

	/* virtual methods from IdleMonitor */
	void OnIdle() noexcept;
};
