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

#ifndef EVENT_EPOLL_BACKEND_HXX
#define EVENT_EPOLL_BACKEND_HXX

#include "system/EpollFD.hxx"

#include <array>
#include <algorithm>

class EpollBackendResult
{
	friend class EpollBackend;

	std::array<epoll_event, 16> events;
	size_t n_events = 0;

public:
	size_t GetSize() const noexcept {
		return n_events;
	}

	unsigned GetEvents(size_t i) const noexcept {
		return events[i].events;
	}

	void *GetObject(size_t i) const noexcept {
		return events[i].data.ptr;
	}
};

class EpollBackend
{
	EpollFD epoll;

	EpollBackend(EpollBackend &) = delete;
	EpollBackend &operator=(EpollBackend &) = delete;
public:
	EpollBackend() = default;

	auto ReadEvents(int timeout_ms) noexcept {
		EpollBackendResult result;
		int ret = epoll.Wait(result.events.data(), result.events.size(),
				     timeout_ms);
		result.n_events = std::max(0, ret);
		return result;
	}

	bool Add(int fd, unsigned events, void *obj) noexcept {
		return epoll.Add(fd, events, obj);
	}

	bool Modify(int fd, unsigned events, void *obj) noexcept {
		return epoll.Modify(fd, events, obj);
	}

	bool Remove(int fd) noexcept {
		return epoll.Remove(fd);
	}

	bool Abandon([[maybe_unused]] int fd) noexcept {
		// Nothing to do in this implementation.
		// Closed descriptors are automatically unregistered.
		return true;
	}
};

#endif
