/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef MPD_EPOLL_FD_HXX
#define MPD_EPOLL_FD_HXX

#include <assert.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdint.h>

#include "check.h"

struct epoll_event;

/**
 * A class that wraps Linux epoll.
 */
class EPollFD {
	const int fd;

public:
	/**
	 * Throws on error.
	 */
	EPollFD();

	~EPollFD() noexcept {
		assert(fd >= 0);

		::close(fd);
	}

	EPollFD(const EPollFD &other) = delete;
	EPollFD &operator=(const EPollFD &other) = delete;

	int Wait(epoll_event *events, int maxevents, int timeout) noexcept {
		return ::epoll_wait(fd, events, maxevents, timeout);
	}

	bool Control(int op, int _fd, epoll_event *event) noexcept {
		return ::epoll_ctl(fd, op, _fd, event) >= 0;
	}

	bool Add(int _fd, uint32_t events, void *ptr) noexcept {
		epoll_event e;
		e.events = events;
		e.data.ptr = ptr;

		return Control(EPOLL_CTL_ADD, _fd, &e);
	}

	bool Modify(int _fd, uint32_t events, void *ptr) noexcept {
		epoll_event e;
		e.events = events;
		e.data.ptr = ptr;

		return Control(EPOLL_CTL_MOD, _fd, &e);
	}

	bool Remove(int _fd) noexcept {
		return Control(EPOLL_CTL_DEL, _fd, nullptr);
	}
};

#endif
