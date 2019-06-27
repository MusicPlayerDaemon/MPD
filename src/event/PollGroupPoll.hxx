/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_EVENT_POLLGROUP_POLL_HXX
#define MPD_EVENT_POLLGROUP_POLL_HXX

#include "PollResultGeneric.hxx"

#include <vector>
#include <unordered_map>

#include <stddef.h>
#include <sys/poll.h>

class PollGroupPoll
{
	struct Item
	{
		size_t index;
		void *obj;
	};

	std::vector<pollfd> poll_events;
	std::unordered_map<int, Item> items;

	PollGroupPoll(PollGroupPoll &) = delete;
	PollGroupPoll &operator=(PollGroupPoll &) = delete;
public:
	static constexpr unsigned READ = POLLIN;
	static constexpr unsigned WRITE = POLLOUT;
	static constexpr unsigned ERROR = POLLERR;
	static constexpr unsigned HANGUP = POLLHUP;

	PollGroupPoll() noexcept;
	~PollGroupPoll() noexcept;

	void ReadEvents(PollResultGeneric &result, int timeout_ms) noexcept;
	bool Add(int fd, unsigned events, void *obj) noexcept;
	bool Modify(int fd, unsigned events, void *obj) noexcept;
	bool Remove(int fd) noexcept;
	bool Abandon(int fd) noexcept {
		return Remove(fd);
	}
};

#endif
