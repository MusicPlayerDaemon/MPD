// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/BindMethod.hxx"
#include "util/IntrusiveList.hxx"

class EventLoop;

/**
 * Defer execution until the next event loop iteration.  Use this to
 * move calls out of the current stack frame, to avoid surprising side
 * effects for callers up in the call chain.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop.
 */
class DeferEvent final : AutoUnlinkIntrusiveListHook
{
	friend class EventLoop;
	friend struct IntrusiveListBaseHookTraits<DeferEvent>;

	EventLoop &loop;

	using Callback = BoundMethod<void() noexcept>;
	const Callback callback;

public:
	DeferEvent(EventLoop &_loop, Callback _callback) noexcept
		:loop(_loop), callback(_callback) {}

	DeferEvent(const DeferEvent &) = delete;
	DeferEvent &operator=(const DeferEvent &) = delete;

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	bool IsPending() const noexcept {
		return is_linked();
	}

	void Schedule() noexcept;

	/**
	 * Schedule this event, but only after the #EventLoop is idle,
	 * i.e. before going to sleep.
	 */
	void ScheduleIdle() noexcept;

	void Cancel() noexcept {
		if (IsPending())
			unlink();
	}

private:
	void Run() noexcept {
		callback();
	}
};
