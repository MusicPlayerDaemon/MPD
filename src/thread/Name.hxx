// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_THREAD_NAME_HXX
#define MPD_THREAD_NAME_HXX

#include "config.h"

#if defined(HAVE_PTHREAD_SETNAME_NP) && !defined(__NetBSD__)
#  define HAVE_THREAD_NAME
#  include <pthread.h>
#elif defined(HAVE_PRCTL)
#  include <sys/prctl.h>
#  ifdef PR_SET_NAME
#    define HAVE_THREAD_NAME
#  endif
#endif

#ifdef HAVE_THREAD_NAME
#include "lib/fmt/ToBuffer.hxx"
#endif

static inline void
SetThreadName(const char *name) noexcept
{
#if defined(HAVE_PTHREAD_SETNAME_NP) && !defined(__NetBSD__)
	/* not using pthread_setname_np() on NetBSD because it
	   requires a non-const pointer argument, which we don't have
	   here */

	pthread_setname_np(pthread_self(), name);
#elif defined(HAVE_PRCTL) && defined(PR_SET_NAME)
	prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
#else
	(void)name;
#endif
}

template<typename... Args>
static inline void
FmtThreadName(const char *fmt, [[maybe_unused]] Args&&... args) noexcept
{
#ifdef HAVE_THREAD_NAME
	SetThreadName(FmtBuffer<16>(fmt, args...));
#else
	(void)fmt;
#endif
}

#endif
