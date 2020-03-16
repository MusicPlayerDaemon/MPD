/*
 * Copyright (C) 2014-2016 Max Kellermann <max.kellermann@gmail.com>
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

#include "Util.hxx"
#include "system/Error.hxx"

#ifdef __linux__
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#ifdef __linux__

#ifndef ANDROID

static int
linux_ioprio_set(int which, int who, int ioprio) noexcept
{
	return syscall(__NR_ioprio_set, which, who, ioprio);
}

static void
ioprio_set_idle() noexcept
{
	static constexpr int _IOPRIO_WHO_PROCESS = 1;
	static constexpr int _IOPRIO_CLASS_IDLE = 3;
	static constexpr int _IOPRIO_CLASS_SHIFT = 13;
	static constexpr int _IOPRIO_IDLE =
		(_IOPRIO_CLASS_IDLE << _IOPRIO_CLASS_SHIFT) | 7;

	linux_ioprio_set(_IOPRIO_WHO_PROCESS, 0, _IOPRIO_IDLE);
}

#endif /* !ANDROID */

/**
 * Wrapper for the "sched_setscheduler" system call.  We don't use the
 * one from the C library because Musl has an intentionally broken
 * implementation.
 */
static int
linux_sched_setscheduler(pid_t pid, int sched,
			 const struct sched_param *param) noexcept
{
	return syscall(__NR_sched_setscheduler, pid, sched, param);
}

#endif

void
SetThreadIdlePriority() noexcept
{
#ifdef __linux__
#ifdef SCHED_IDLE
	static struct sched_param sched_param;
	linux_sched_setscheduler(0, SCHED_IDLE, &sched_param);
#endif

#ifndef ANDROID
	/* this system call is forbidden via seccomp on Android 8 and
	   leads to crash (SIGSYS) */
	ioprio_set_idle();
#endif

#elif defined(_WIN32)
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
#endif
}

void
SetThreadRealtime()
{
#ifdef __linux__
	struct sched_param sched_param;
	sched_param.sched_priority = 40;

	int policy = SCHED_FIFO;
#ifdef SCHED_RESET_ON_FORK
	policy |= SCHED_RESET_ON_FORK;
#endif

	if (linux_sched_setscheduler(0, policy, &sched_param) < 0)
		throw MakeErrno("sched_setscheduler failed");
#endif	// __linux__
}
