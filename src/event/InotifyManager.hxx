// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "InotifyEvent.hxx"
#include "util/IntrusiveHashSet.hxx"

class InotifyManager;

/**
 * Watch for one file.  Managed by #InotifyManager.
 */
class InotifyWatch {
	friend class InotifyManager;

	IntrusiveHashSetHook<IntrusiveHookMode::NORMAL> watch_descriptor_hook;

	InotifyManager &manager;

	int watch_descriptor = -1;

	bool oneshot;

public:
	explicit InotifyWatch(InotifyManager &_manager) noexcept
		:manager(_manager) {}

	~InotifyWatch() noexcept {
		RemoveWatch();
	}

	InotifyWatch(const InotifyWatch &) = delete;
	InotifyWatch &operator=(const InotifyWatch &) = delete;

	auto &GetManager() const noexcept {
		return manager;
	}

	bool IsWatching() const noexcept {
		return watch_descriptor >= 0;
	}

	bool TryAddWatch(const char *pathname, uint32_t mask) noexcept;
	void AddWatch(const char *pathname, uint32_t mask);
	void RemoveWatch() noexcept;

protected:
	/**
	 * An inotify event was received.
	 */
	virtual void OnInotify(unsigned mask, const char *name) noexcept = 0;
 };

/**
 * Wrapper for #InotifyEvent with a watch descriptor manager.
 */
class InotifyManager final : InotifyHandler {
	friend class InotifyWatch;

	InotifyEvent event;

	struct GetWatchDescriptor {
		constexpr int operator()(const InotifyWatch &w) const noexcept {
			return w.watch_descriptor;
		}
	};

	IntrusiveHashSet<InotifyWatch, 1024,
			 IntrusiveHashSetOperators<InotifyWatch, GetWatchDescriptor,
						   std::hash<int>,
						   std::equal_to<int>>,
			 IntrusiveHashSetMemberHookTraits<&InotifyWatch::watch_descriptor_hook>> watches;

public:
	/**
	 * Throws on error.
	 */
	explicit InotifyManager(EventLoop &event_loop);

	~InotifyManager() noexcept;

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	/**
	 * Is the inotify file descriptor still open?
	 */
	bool IsDefined() const noexcept {
		return event.IsDefined();
	}

	/**
	 * Initiate shutdown.  This unregisters all #EventLoop events
	 * and prevents new ones from getting registered.
	 */
	void BeginShutdown() noexcept;

	/**
	 * Has BeginShutdown() been called?
	 */
	bool IsShuttingDown() const noexcept {
		return !event.IsDefined();
	}

private:
	// virtual methods from InotifyHandler
	void OnInotify(int wd, unsigned mask, const char *name) override;
	void OnInotifyError(std::exception_ptr error) noexcept override;
};
