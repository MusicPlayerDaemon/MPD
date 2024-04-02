// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Chrono.hxx"
#include "event/Features.h"
#include "util/IntrusiveTreeSet.hxx"

class FineTimerEvent;

/**
 * A list of #FineTimerEvent instances sorted by due time point.
 */
class TimerList final {
	struct GetDue {
		constexpr Event::TimePoint operator()(const FineTimerEvent &timer) const noexcept;
	};

	IntrusiveTreeSet<FineTimerEvent,
			 IntrusiveTreeSetOperators<FineTimerEvent, GetDue>> timers;

public:
	TimerList();
	~TimerList() noexcept;

	TimerList(const TimerList &other) = delete;
	TimerList &operator=(const TimerList &other) = delete;

	bool IsEmpty() const noexcept {
		return timers.empty();
	}

	void Insert(FineTimerEvent &t) noexcept;

	/**
	 * Invoke all expired #FineTimerEvent instances and return the
	 * duration until the next timer expires.  Returns a negative
	 * duration if there is no timeout.
	 */
	Event::Duration Run(Event::TimePoint now) noexcept;
};
