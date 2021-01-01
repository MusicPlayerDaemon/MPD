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

#ifndef EVENT_POLL_BACKEND_HXX
#define EVENT_POLL_BACKEND_HXX

#include "PollResultGeneric.hxx"

#include <cstddef>
#include <vector>
#include <unordered_map>

#include <sys/poll.h>

class PollBackend
{
	struct Item
	{
		std::size_t index;
		void *obj;

		constexpr Item(std::size_t _index, void *_obj) noexcept
			:index(_index), obj(_obj) {}

		Item(const Item &) = delete;
		Item &operator=(const Item &) = delete;
	};

	std::vector<pollfd> poll_events;
	std::unordered_map<int, Item> items;

	PollBackend(PollBackend &) = delete;
	PollBackend &operator=(PollBackend &) = delete;
public:
	PollBackend() noexcept;
	~PollBackend() noexcept;

	PollResultGeneric ReadEvents(int timeout_ms) noexcept;
	bool Add(int fd, unsigned events, void *obj) noexcept;
	bool Modify(int fd, unsigned events, void *obj) noexcept;
	bool Remove(int fd) noexcept;
	bool Abandon(int fd) noexcept {
		return Remove(fd);
	}
};

#endif
