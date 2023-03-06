// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPDATE_SERVICE_HXX
#define MPD_UPDATE_SERVICE_HXX

#include "Config.hxx"
#include "Queue.hxx"
#include "event/InjectEvent.hxx"
#include "thread/Thread.hxx"

#include <memory>
#include <string_view>

class SimpleDatabase;
class DatabaseListener;
class UpdateWalk;
class CompositeStorage;

/**
 * This class manages the update queue and runs the update thread.
 */
class UpdateService final {
	const UpdateConfig config;

	InjectEvent defer;

	SimpleDatabase &db;
	CompositeStorage &storage;

	DatabaseListener &listener;

	bool modified;

	Thread update_thread;

	static constexpr unsigned update_task_id_max = 1 << 15;

	unsigned update_task_id = 0;

	UpdateQueue queue;

	UpdateQueueItem next;

	std::unique_ptr<UpdateWalk> walk;

public:
	UpdateService(const ConfigData &_config,
		      EventLoop &_loop, SimpleDatabase &_db,
		      CompositeStorage &_storage,
		      DatabaseListener &_listener) noexcept;

	~UpdateService() noexcept;

	auto &GetEventLoop() const noexcept {
		return defer.GetEventLoop();
	}

	/**
	 * Returns a non-zero job id when we are currently updating
	 * the database.
	 */
	unsigned GetId() const noexcept {
		return next.id;
	}

	/**
	 * Add this path to the database update queue.
	 *
	 * Throws on error
	 *
	 * @param path a path to update; if an empty string,
	 * the whole music directory is updated
	 * @return the job id
	 */
	unsigned Enqueue(std::string_view path, bool discard);

	/**
	 * Clear the queue and cancel the current update.  Does not
	 * wait for the thread to exit.
	 */
	void CancelAllAsync() noexcept;

	/**
	 * Cancel all updates for the given mount point.  If an update
	 * is already running for it, the method will wait for
	 * cancellation to complete.
	 */
	void CancelMount(const char *uri) noexcept;

private:
	/* InjectEvent callback */
	void RunDeferred() noexcept;

	/* the update thread */
	void Task() noexcept;

	void StartThread(UpdateQueueItem &&i);

	unsigned GenerateId() noexcept;
};

#endif
