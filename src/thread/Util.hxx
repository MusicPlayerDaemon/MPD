/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_THREAD_UTIL_HXX
#define MPD_THREAD_UTIL_HXX

#ifdef __linux__
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(WIN32)
#include <windows.h>
#endif

#ifdef __linux__

static int
ioprio_set(int which, int who, int ioprio)
{
	return syscall(SYS_ioprio_set, which, who, ioprio);
}

static void
ioprio_set_idle()
{
	static constexpr int _IOPRIO_WHO_PROCESS = 1;
	static constexpr int _IOPRIO_CLASS_IDLE = 3;
	static constexpr int _IOPRIO_CLASS_SHIFT = 13;
	static constexpr int _IOPRIO_IDLE =
		(_IOPRIO_CLASS_IDLE << _IOPRIO_CLASS_SHIFT) | 7;

	ioprio_set(_IOPRIO_WHO_PROCESS, 0, _IOPRIO_IDLE);
}

#endif

/**
 * Lower the current thread's priority to "idle" (very low).
 */
static inline void
SetThreadIdlePriority()
{
#ifdef __linux__
#ifdef SCHED_IDLE
	static struct sched_param sched_param;
	sched_setscheduler(0, SCHED_IDLE, &sched_param);
#endif

	ioprio_set_idle();

#elif defined(WIN32)
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
#endif
};

/**
 * Raise the current thread's priority to "real-time" (very high).
 */
static inline void
SetThreadRealtime()
{
#ifdef __linux__
	struct sched_param sched_param;
	sched_param.sched_priority = 50;
	sched_setscheduler(0, SCHED_FIFO|SCHED_RESET_ON_FORK, &sched_param);
#endif
};

#endif
