// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Chrono.hxx"
#include "event/Features.h"
#include "util/BindMethod.hxx"

#ifdef NO_BOOST
#include "util/IntrusiveList.hxx"
#else
#include <boost/intrusive/set_hook.hpp>
#endif

#include <cassert>

class EventLoop;

/**
 * This class invokes a callback function after a certain amount of
 * time.  Use Schedule() to start the timer or Cancel() to cancel it.
 *
 * Unlike #CoarseTimerEvent, this class uses a high-resolution timer,
 * but at the cost of more expensive insertion and deletion.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class FineTimerEvent final :
#ifdef NO_BOOST
	AutoUnlinkIntrusiveListHook
#else
	public boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
#endif
{
	friend class TimerList;
#ifdef NO_BOOST
	friend struct IntrusiveListBaseHookTraits<FineTimerEvent>;
#endif

	EventLoop &loop;

	using Callback = BoundMethod<void() noexcept>;
	const Callback callback;

	/**
	 * When is this timer due?  This is only valid if IsPending()
	 * returns true.
	 */
	Event::TimePoint due;

public:
	FineTimerEvent(EventLoop &_loop, Callback _callback) noexcept
		:loop(_loop), callback(_callback) {}

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	constexpr auto GetDue() const noexcept {
		return due;
	}

	/**
	 * Set the due time as an absolute time point.  This can be
	 * done to prepare an eventual ScheduleCurrent() call.  Must
	 * not be called while the timer is already scheduled.
	 */
	void SetDue(Event::TimePoint _due) noexcept {
		assert(!IsPending());

		due = _due;
	}

	/**
	 * Set the due time as a duration relative to now.  This can
	 * done to prepare an eventual ScheduleCurrent() call.  Must
	 * not be called while the timer is already scheduled.
	 */
	void SetDue(Event::Duration d) noexcept;

	/**
	 * Was this timer scheduled?
	 */
	bool IsPending() const noexcept {
		return is_linked();
	}

	/**
	 * Schedule the timer at the due time that was already set;
	 * either by SetDue() or by a Schedule() call that was already
	 * canceled.
	 */
	void ScheduleCurrent() noexcept;

	void Schedule(Event::Duration d) noexcept;

	/**
	 * Like Schedule(), but is a no-op if there is a due time
	 * earlier than the given one.
	 */
	void ScheduleEarlier(Event::Duration d) noexcept;

	void Cancel() noexcept {
#ifdef NO_BOOST
		if (IsPending())
#endif
			unlink();
	}

private:
	void Run() noexcept {
		callback();
	}
};
