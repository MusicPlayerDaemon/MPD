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

#ifndef MPD_EVENT_POLLGROUP_WINSELECT_HXX
#define MPD_EVENT_POLLGROUP_WINSELECT_HXX

#include "PollResultGeneric.hxx"

#include <assert.h>
#include <string.h>

#include <unordered_map>

#include <windows.h>
#include <winsock2.h>

/* ERROR is a WIN32 macro that poisons our namespace; this is a kludge
   to allow us to use it anyway */
#ifdef ERROR
#undef ERROR
#endif

class SocketSet
{
	fd_set set;
public:
	SocketSet() noexcept {
		set.fd_count = 0;
	}

	SocketSet(const SocketSet &other) noexcept {
		set.fd_count = other.set.fd_count;
		memcpy(set.fd_array,
		       other.set.fd_array,
		       sizeof (SOCKET) * set.fd_count);
	}

	fd_set *GetPtr() noexcept {
		return &set;
	}

	size_t Size() const noexcept {
		return set.fd_count;
	}

	bool IsEmpty() const noexcept {
		return set.fd_count == 0;
	}

	bool IsFull() const noexcept {
		return set.fd_count == FD_SETSIZE;
	}

	int operator[](size_t index) const noexcept {
		assert(index < set.fd_count);
		return set.fd_array[index];
	}

	size_t Add(int fd) noexcept {
		assert(!IsFull());
		set.fd_array[set.fd_count] = fd;
		return set.fd_count++;
	}

	void MoveToEnd(size_t index) noexcept {
		assert(index < set.fd_count);
		std::swap(set.fd_array[index], set.fd_array[set.fd_count - 1]);
	}

	void RemoveLast() noexcept {
		assert(!IsEmpty());
		--set.fd_count;
	}
};

class PollGroupWinSelect
{
	struct Item
	{
		int index[2];
		void *obj;
		unsigned events;
	};

	SocketSet event_set[2];
	std::unordered_map<int, Item> items;

	bool CanModify(Item &item, unsigned events,
		       int event_id) const noexcept;
	void Modify(Item &item, int fd, unsigned events,
		    int event_id) noexcept;

	PollGroupWinSelect(PollGroupWinSelect &) = delete;
	PollGroupWinSelect &operator=(PollGroupWinSelect &) = delete;
public:
	static constexpr unsigned READ = 1;
	static constexpr unsigned WRITE = 2;
	static constexpr unsigned ERROR = 0;
	static constexpr unsigned HANGUP = 0;

	PollGroupWinSelect() noexcept;
	~PollGroupWinSelect() noexcept;

	void ReadEvents(PollResultGeneric &result, int timeout_ms) noexcept;
	bool Add(int fd, unsigned events, void *obj) noexcept;
	bool Modify(int fd, unsigned events, void *obj) noexcept;
	bool Remove(int fd) noexcept;
	bool Abandon(int fd) noexcept {
		return Remove(fd);
	}
};

#endif
