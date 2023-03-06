// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include "Chrono.hxx"
#include "event/Features.h"

#ifdef NO_BOOST
#include "util/IntrusiveSortedList.hxx"
#else
#include <boost/intrusive/set.hpp>
#endif

class FineTimerEvent;

/**
 * A list of #FineTimerEvent instances sorted by due time point.
 */
class TimerList final {
	struct Compare {
		constexpr bool operator()(const FineTimerEvent &a,
					  const FineTimerEvent &b) const noexcept;
	};

#ifdef NO_BOOST
	/* when building without Boost, then this is just a sorted
	   doubly-linked list - this doesn't scale well, but is good
	   enough for most programs */
	IntrusiveSortedList<FineTimerEvent, Compare> timers;
#else
	boost::intrusive::multiset<FineTimerEvent,
				   boost::intrusive::base_hook<boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>>,
				   boost::intrusive::compare<Compare>,
				   boost::intrusive::constant_time_size<false>> timers;
#endif

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
