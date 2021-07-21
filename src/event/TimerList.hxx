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
#include "event/Features.h"
#include "util/IntrusiveList.hxx"

#ifndef NO_BOOST
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
	IntrusiveList<FineTimerEvent> timers;
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
