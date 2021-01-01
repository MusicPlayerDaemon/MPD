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
#include "util/StringFormat.hxx"
#endif

static inline void
SetThreadName(const char *name) noexcept
{
#if defined(HAVE_PTHREAD_SETNAME_NP) && !defined(__NetBSD__)
	/* not using pthread_setname_np() on NetBSD because it
	   requires a non-const pointer argument, which we don't have
	   here */

#ifdef __APPLE__
	pthread_setname_np(name);
#else
	pthread_setname_np(pthread_self(), name);
#endif
#elif defined(HAVE_PRCTL) && defined(PR_SET_NAME)
	prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
#else
	(void)name;
#endif
}

template<typename... Args>
static inline void
FormatThreadName(const char *fmt, [[maybe_unused]] Args&&... args) noexcept
{
#ifdef HAVE_THREAD_NAME
	SetThreadName(StringFormat<16>(fmt, args...));
#else
	(void)fmt;
#endif
}

#endif
