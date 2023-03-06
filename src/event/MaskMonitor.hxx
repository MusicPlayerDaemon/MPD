// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_EVENT_MASK_MONITOR_HXX
#define MPD_EVENT_MASK_MONITOR_HXX

#include "InjectEvent.hxx"
#include "util/BindMethod.hxx"

#include <atomic>

/**
 * Manage a bit mask of events that have occurred.  Every time the
 * mask becomes non-zero, OnMask() is called in #EventLoop's thread.
 *
 * This class is thread-safe.
 */
class MaskMonitor final {
	InjectEvent event;

	using Callback = BoundMethod<void(unsigned) noexcept>;
	const Callback callback;

	std::atomic_uint pending_mask;

public:
	MaskMonitor(EventLoop &_loop, Callback _callback) noexcept
		:event(_loop, BIND_THIS_METHOD(RunDeferred)),
		 callback(_callback), pending_mask(0) {}

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Cancel() noexcept {
		event.Cancel();
	}

	void OrMask(unsigned new_mask) noexcept;

protected:
	/* InjectEvent callback */
	void RunDeferred() noexcept;
};

#endif
