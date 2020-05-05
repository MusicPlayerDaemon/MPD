/*
 * Copyright 2013-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef EPOLL_FD_HXX
#define EPOLL_FD_HXX

#include "io/UniqueFileDescriptor.hxx"

#include <cstdint>

#include <sys/epoll.h>

/**
 * A class that wraps Linux epoll.
 */
class EpollFD {
	UniqueFileDescriptor fd;

public:
	/**
	 * Throws on error.
	 */
	EpollFD();

	EpollFD(EpollFD &&) = default;
	EpollFD &operator=(EpollFD &&) = default;

	int Wait(epoll_event *events, int maxevents, int timeout) noexcept {
		return ::epoll_wait(fd.Get(), events, maxevents, timeout);
	}

	bool Control(int op, int _fd, epoll_event *event) noexcept {
		return ::epoll_ctl(fd.Get(), op, _fd, event) >= 0;
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
