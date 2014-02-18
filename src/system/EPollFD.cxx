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

#include "config.h"
#ifdef USE_EPOLL
#include "EPollFD.hxx"
#include "FatalError.hxx"

#ifdef __BIONIC__

#include <sys/syscall.h>
#include <fcntl.h>

#define EPOLL_CLOEXEC O_CLOEXEC

static inline int
epoll_create1(int flags)
{
    return syscall(__NR_epoll_create1, flags);
}

#endif

EPollFD::EPollFD()
	:fd(::epoll_create1(EPOLL_CLOEXEC))
{
	if (fd < 0)
		FatalSystemError("epoll_create1() failed");
}

#endif /* USE_EPOLL */
