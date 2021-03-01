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
#include "util/IntrusiveList.hxx"

#include <array>
#include <algorithm>

class CoarseTimerEvent;

/**
 * A list of #CoarseTimerEvent instances managed in a circular timer
 * wheel.
 */
class TimerWheel final {
	static constexpr Event::Duration RESOLUTION = std::chrono::seconds(1);
	static constexpr Event::Duration SPAN = std::chrono::minutes(2);

	static_assert(SPAN % RESOLUTION == Event::Duration::zero());

	static constexpr std::size_t N_BUCKETS = SPAN / RESOLUTION;

	using List = IntrusiveList<CoarseTimerEvent>;

	/**
	 * Each bucket contains a doubly linked list of
	 * #CoarseTimerEvent instances scheduled for one #RESOLUTION.
	 *
	 * Timers scheduled far into the future (more than #SPAN) may
	 * also sit in between, so anybody walking those lists should
	 * check the due time.
	 */
	std::array<List, N_BUCKETS> buckets;

	/**
	 * A list of timers which are already ready.  This can happen
	 * if they are scheduled with a zero duration or scheduled in
	 * the past.
	 */
	List ready;

	/**
	 * The last time Run() was invoked.  This is needed to
	 * determine the range of buckets to be checked, because we
	 * can't rely on getting a caller for every bucket; there may
	 * be arbitrary delays.
	 */
	Event::TimePoint last_time{};

	/**
	 * If this flag is true, then all buckets are guaranteed to be
	 * empty.  If it is false, the buckets may or may not be
	 * empty; if so, the next full scan will set it back to true.
	 *
	 * This field is "mutable" so the "const" method GetSleep()
	 * can update it.
	 */
	mutable bool empty = true;

public:
	TimerWheel() noexcept;
	~TimerWheel() noexcept;

	bool IsEmpty() const noexcept {
		return ready.empty() &&
			std::all_of(buckets.begin(), buckets.end(),
				    [](const auto &list){
					    return list.empty();
				    });
	}

	void Insert(CoarseTimerEvent &t,
		    Event::TimePoint now) noexcept;

	/**
	 * Invoke all expired #CoarseTimerEvent instances and return
	 * the duration until the next timer expires.  Returns a
	 * negative duration if there is no timeout.
	 */
	Event::Duration Run(Event::TimePoint now) noexcept;

private:
	static constexpr std::size_t NextBucketIndex(std::size_t i) noexcept {
		return (i + 1) % N_BUCKETS;
	}

	static constexpr std::size_t BucketIndexAt(Event::TimePoint t) noexcept {
		return std::size_t(t.time_since_epoch() / RESOLUTION)
			% N_BUCKETS;
	}

	static constexpr Event::TimePoint GetBucketStartTime(Event::TimePoint t) noexcept {
		return t - t.time_since_epoch() % RESOLUTION;
	}

	/**
	 * What is the end time of the next non-empty bucket?
	 *
	 * @param bucket_index start searching at this bucket index
	 * @return the bucket end time or max() if the wheel is empty
	 */
	[[gnu::pure]]
	Event::TimePoint GetNextDue(std::size_t bucket_index,
				    Event::TimePoint bucket_start_time) const noexcept;

	[[gnu::pure]]
	Event::Duration GetSleep(Event::TimePoint now) const noexcept;

	/**
	 * Run all due timers in this bucket.
	 */
	static void Run(List &list, Event::TimePoint now) noexcept;
};
