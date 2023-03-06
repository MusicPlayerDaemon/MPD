// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_NFS_MANAGER_HXX
#define MPD_NFS_MANAGER_HXX

#include "Connection.hxx"
#include "event/IdleEvent.hxx"
#include "util/IntrusiveList.hxx"

/**
 * A manager for NFS connections.  Handles multiple connections to
 * multiple NFS servers.
 */
class NfsManager final {
	class ManagedConnection final
		: public NfsConnection,
		  public IntrusiveListHook<>
	{
		NfsManager &manager;

	public:
		ManagedConnection(NfsManager &_manager, EventLoop &_loop,
				  const char *_server,
				  const char *_export_name) noexcept
			:NfsConnection(_loop, _server, _export_name),
			 manager(_manager) {}

	protected:
		/* virtual methods from NfsConnection */
		void OnNfsConnectionError(std::exception_ptr &&e) noexcept override;
	};

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
	explicit NfsManager(EventLoop &_loop) noexcept
		:idle_event(_loop, BIND_THIS_METHOD(OnIdle)) {}

	/**
	 * Must be run from EventLoop's thread.
	 */
	~NfsManager() noexcept;

	auto &GetEventLoop() const noexcept {
		return idle_event.GetEventLoop();
	}

	[[gnu::pure]]
	NfsConnection &GetConnection(const char *server,
				     const char *export_name) noexcept;

private:
	void ScheduleDelete(ManagedConnection &c) noexcept {
		connections.erase(connections.iterator_to(c));
		garbage.push_front(c);
		idle_event.Schedule();
	}

	/**
	 * Delete all connections on the #garbage list.
	 */
	void CollectGarbage() noexcept;

	/* virtual methods from IdleMonitor */
	void OnIdle() noexcept;
};

#endif
