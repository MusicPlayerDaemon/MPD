/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
