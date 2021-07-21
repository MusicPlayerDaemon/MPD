/*
 * Copyright 2007-2021 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Chrono.hxx"
#include "util/BindMethod.hxx"
#include "util/IntrusiveList.hxx"

class EventLoop;

/**
 * This class invokes a callback function after a certain amount of
 * time.  Use Schedule() to start the timer or Cancel() to cancel it.
 *
 * Unlike #FineTimerEvent, this class has a granularity of about 1
 * second, and is optimized for timeouts between 1 and 60 seconds
 * which are often canceled before they expire (i.e. optimized for
 * fast insertion and deletion, at the cost of granularity).
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class CoarseTimerEvent final : AutoUnlinkIntrusiveListHook
{
	friend class TimerWheel;
	friend class IntrusiveList<CoarseTimerEvent>;

	EventLoop &loop;

	using Callback = BoundMethod<void() noexcept>;
	const Callback callback;

	/**
	 * When is this timer due?  This is only valid if IsPending()
	 * returns true.
	 */
	Event::TimePoint due;

public:
	CoarseTimerEvent(EventLoop &_loop, Callback _callback) noexcept
		:loop(_loop), callback(_callback) {}

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	constexpr auto GetDue() const noexcept {
		return due;
	}

	bool IsPending() const noexcept {
		return is_linked();
	}

	void Schedule(Event::Duration d) noexcept;

	/**
	 * Like Schedule(), but is a no-op if there is a due time
	 * earlier than the given one.
	 */
	void ScheduleEarlier(Event::Duration d) noexcept;

	void Cancel() noexcept {
		if (IsPending())
			unlink();
	}

private:
	void Run() noexcept {
		callback();
	}
};
