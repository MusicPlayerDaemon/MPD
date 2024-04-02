// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "TimerList.hxx"
#include "FineTimerEvent.hxx"

constexpr Event::TimePoint
TimerList::GetDue::operator()(const FineTimerEvent &timer) const noexcept
{
	return timer.due;
}

#ifdef NO_BOOST

constexpr bool
TimerList::Compare::operator()(const FineTimerEvent &a,
			       const FineTimerEvent &b) const noexcept
{
	return a.due < b.due;
}

#endif

TimerList::TimerList() = default;

TimerList::~TimerList() noexcept
{
	assert(timers.empty());
}

void
TimerList::Insert(FineTimerEvent &t) noexcept
{
	timers.insert(t);
}

Event::Duration
TimerList::Run(const Event::TimePoint now) noexcept
{
	while (true) {
		auto i = timers.begin();
		if (i == timers.end())
			break;

		auto &t = *i;
		const auto timeout = t.due - now;
		if (timeout > timeout.zero())
			return timeout;

#ifdef NO_BOOST
		t.Cancel();
#else
		timers.pop_front();
#endif

		t.Run();
	}

	return Event::Duration(-1);
}
