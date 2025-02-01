// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

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

	FileDescriptor GetFileDescriptor() const noexcept {
		return fd;
	}

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
