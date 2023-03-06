// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INJECT_EVENT_HXX
#define MPD_INJECT_EVENT_HXX

#include "util/BindMethod.hxx"
#include "util/IntrusiveList.hxx"

class EventLoop;

/**
 * Invoke a method call in the #EventLoop.
 *
 * This class is thread-safe.
 */
class InjectEvent final : public SafeLinkIntrusiveListHook
{
	friend class EventLoop;

	EventLoop &loop;

	using Callback = BoundMethod<void() noexcept>;
	const Callback callback;

public:
	InjectEvent(EventLoop &_loop, Callback _callback) noexcept
		:loop(_loop), callback(_callback) {}

	~InjectEvent() noexcept {
		Cancel();
	}

	EventLoop &GetEventLoop() const noexcept {
		return loop;
	}

	void Schedule() noexcept;
	void Cancel() noexcept;

private:
	bool IsPending() const noexcept {
		return is_linked();
	}

	void Run() noexcept {
		callback();
	}
};

#endif
