// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <compare>

#include "Chrono.hxx"
#include "event/Features.h"
#include "util/IntrusiveTreeSet.hxx"

class FineTimerEvent;

struct custom_compare {
  auto operator()(const std::chrono::steady_clock::time_point& lhs,
                  const std::chrono::steady_clock::time_point& rhs) const {
    return lhs < rhs ? std::strong_ordering::less
           : lhs == rhs ? std::strong_ordering::equal
                        : std::strong_ordering::greater;
  }
};

/**
 * A list of #FineTimerEvent instances sorted by due time point.
 */
class TimerList final {
	struct GetDue {
		constexpr Event::TimePoint operator()(const FineTimerEvent &timer) const noexcept;
	};

	IntrusiveTreeSet<FineTimerEvent,
			 IntrusiveTreeSetOperators<FineTimerEvent, GetDue, custom_compare>> timers;

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
