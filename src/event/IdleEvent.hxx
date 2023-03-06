// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SOCKET_IDLE_EVENT_HXX
#define MPD_SOCKET_IDLE_EVENT_HXX

#include "DeferEvent.hxx"

class EventLoop;

/**
 * An event that runs when the EventLoop has become idle, before
 * waiting for more events.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class IdleEvent final {
	DeferEvent event;

	using Callback = BoundMethod<void() noexcept>;

public:
	IdleEvent(EventLoop &_loop, Callback _callback) noexcept
		:event(_loop, _callback) {}

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	bool IsPending() const noexcept {
		return event.IsPending();
	}

	void Schedule() noexcept {
		event.ScheduleIdle();
	}

	void Cancel() noexcept {
		event.Cancel();
	}
};

#endif
