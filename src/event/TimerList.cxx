// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Loop.hxx"
#include "FineTimerEvent.hxx"

#ifdef NO_BOOST
#include <algorithm>
#endif

constexpr bool
TimerList::Compare::operator()(const FineTimerEvent &a,
			       const FineTimerEvent &b) const noexcept
{
	return a.due < b.due;
}

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
		timers.erase(i);
#endif

		t.Run();
	}

	return Event::Duration(-1);
}
