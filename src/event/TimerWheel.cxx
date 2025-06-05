// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "TimerWheel.hxx"
#include "CoarseTimerEvent.hxx"

#include <cassert>

TimerWheel::TimerWheel() noexcept
{
	/* cannot use "=default" due to bug in GCC9 and older
	   ("defaulted on its redeclaration with an
	   exception-specification that differs from the implicit
	   exception-specification") */
}

TimerWheel::~TimerWheel() noexcept = default;

void
TimerWheel::Insert(CoarseTimerEvent &t,
		   Event::TimePoint now) noexcept
{
	auto &list = t.GetDue() > now
		? buckets[BucketIndexAt(t.GetDue())]
		/* if this timer is already due, insert it into the
		   "ready" list to be invoked without delay */
		: ready;

	list.push_back(t);

	empty = false;
}

void
TimerWheel::Run(List &list, Event::TimePoint now) noexcept
{
	/* move all timers to a temporary list to avoid problems with
	   canceled timers while we traverse the list */
	auto tmp = std::move(list);

	tmp.clear_and_dispose([&](auto *t){
		if (t->GetDue() <= now) {
			/* this timer is due: run it */
			t->Run();
		} else {
			/* not yet due: move it back to the given
			   list */
			list.push_back(*t);
		}
	});
}

inline Event::TimePoint
TimerWheel::GetNextDue(const std::size_t bucket_index,
		       const Event::TimePoint bucket_start_time) const noexcept
{
	Event::TimePoint t = bucket_start_time;

	for (std::size_t i = bucket_index;;) {
		t += RESOLUTION;

		if (!buckets[i].empty())
			/* found a non-empty bucket; return this
			   bucket's end time */
			return t;

		i = NextBucketIndex(i);
		if (i == bucket_index)
			/* no timer scheduled - no wakeup */
			return Event::TimePoint::max();
	}
}

inline Event::Duration
TimerWheel::GetSleep(Event::TimePoint now) const noexcept
{
	/* note: not checking the "ready" list here because this
	   method gets called only from Run() after the "ready" list
	   has been processed already */

	if (empty)
		return Event::Duration(-1);

	auto t = GetNextDue(BucketIndexAt(now), GetBucketStartTime(now));
	assert(t > now);
	if (t == Event::TimePoint::max()) {
		empty = true;
		return Event::Duration(-1);
	}

	return t - now;
}

Event::Duration
TimerWheel::Run(const Event::TimePoint now) noexcept
{
	/* invoke the "ready" list unconditionally */
	ready.clear_and_dispose([&](auto *t){
		t->Run();
	});

	/* check all buckets between the last time we were invoked and
	   now */
	const std::size_t start_bucket = BucketIndexAt(last_time);
	std::size_t end_bucket;

	if (now < last_time || now >= last_time + SPAN - RESOLUTION) {
		/* too much time has passed (or time warp): check all
		   buckets */
		end_bucket = start_bucket;
	} else {
		/* check only the relevant range of buckets (between
		   the last run and now) */
		/* note, we're not checking the current bucket index,
		   we stop at the one before that; all timers in the
		   same bucket shall be combined, so we only execute
		   it when the bucket end has passed by */
		end_bucket = BucketIndexAt(now);

		if (start_bucket == end_bucket)
			/* still on the same bucket - don't run any
			   timers, instead wait until this bucket end
			   has passed by */
			return GetSleep(now);
	}

	last_time = GetBucketStartTime(now);
	assert(BucketIndexAt(last_time) == BucketIndexAt(now));

	/* run those buckets */

	for (std::size_t i = start_bucket;;) {
		Run(buckets[i], now);

		i = NextBucketIndex(i);
		if (i == end_bucket)
			break;
	}

	/* now determine how much time remains until the next
	   non-empty bucket passes */

	return GetSleep(now);
}
