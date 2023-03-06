// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
