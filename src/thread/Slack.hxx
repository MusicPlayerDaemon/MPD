// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_THREAD_SLACK_HXX
#define MPD_THREAD_SLACK_HXX

#include "config.h"

#include <chrono>

#ifdef HAVE_PRCTL
#include <sys/prctl.h>
#endif

/**
 * Set the current thread's timer slack to the specified number of
 * nanoseconds (requires Linux 2.6.28).  This allows the kernel to
 * merge multiple wakeups, which is a trick to save energy.
 */
static inline void
SetThreadTimerSlackNS(unsigned long slack_ns) noexcept
{
#if defined(HAVE_PRCTL) && defined(PR_SET_TIMERSLACK)
	prctl(PR_SET_TIMERSLACK, slack_ns, 0, 0, 0);
#else
	(void)slack_ns;
#endif
}

template<class Rep, class Period>
static inline auto
SetThreadTimerSlack(const std::chrono::duration<Rep,Period> &slack) noexcept
{
	SetThreadTimerSlackNS(std::chrono::duration_cast<std::chrono::nanoseconds>(slack).count());
}

#endif
