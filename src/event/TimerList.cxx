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
#ifdef NO_BOOST
	auto i = std::find_if(timers.begin(), timers.end(), [due = t.GetDue()](const auto &other){
		return other.GetDue() >= due;
	});

	timers.insert(i, t);
#else
	timers.insert(t);
#endif
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
