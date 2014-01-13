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

#ifndef MPD_EVENT_POLLGROUP_EPOLL_HXX
#define MPD_EVENT_POLLGROUP_EPOLL_HXX

#include "check.h"

#include "Compiler.h"
#include "system/EPollFD.hxx"

#include <array>
#include <algorithm>

class PollResultEPoll
{
	friend class PollGroupEPoll;

	std::array<epoll_event, 16> events;
	int n_events;
public:
	PollResultEPoll() : n_events(0) { }

	int GetSize() const { return n_events; }
	unsigned GetEvents(int i) const { return events[i].events; }
	void *GetObject(int i) const { return events[i].data.ptr; }
	void Reset() { n_events = 0; }

	void Clear(void *obj) {
		for (int i = 0; i < n_events; ++i)
			if (events[i].data.ptr == obj)
				events[i].events = 0;
	}
};

class PollGroupEPoll
{
	EPollFD epoll;

	PollGroupEPoll(PollGroupEPoll &) = delete;
	PollGroupEPoll &operator=(PollGroupEPoll &) = delete;
public:
	static constexpr unsigned READ = EPOLLIN;
	static constexpr unsigned WRITE = EPOLLOUT;
	static constexpr unsigned ERROR = EPOLLERR;
	static constexpr unsigned HANGUP = EPOLLHUP;

	PollGroupEPoll() = default;

	void ReadEvents(PollResultEPoll &result, int timeout_ms) {
		int ret = epoll.Wait(result.events.data(), result.events.size(),
				     timeout_ms);
		result.n_events = std::max(0, ret);
	}

	bool Add(int fd, unsigned events, void *obj) {
		return epoll.Add(fd, events, obj);
	}

	bool Modify(int fd, unsigned events, void *obj) {
		return epoll.Modify(fd, events, obj);
	}

	bool Remove(int fd) {
		return epoll.Remove(fd);
	}

	bool Abandon(gcc_unused int fd) {
		// Nothing to do in this implementation.
		// Closed descriptors are automatically unregistered.
		return true;
	}
};

#endif
