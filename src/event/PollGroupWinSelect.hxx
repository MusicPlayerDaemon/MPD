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

#ifndef MPD_EVENT_POLLGROUP_WINSELECT_HXX
#define MPD_EVENT_POLLGROUP_WINSELECT_HXX

#include "check.h"

#include "PollResultGeneric.hxx"

#include <assert.h>
#include <string.h>

#include <unordered_map>

#include <windows.h>
#include <winsock2.h>

#ifdef ERROR
#undef ERROR
#endif

class SocketSet
{
	fd_set set;
public:
	SocketSet() { set.fd_count = 0; }
	SocketSet(SocketSet &other) {
		set.fd_count = other.set.fd_count;
		memcpy(set.fd_array,
		       other.set.fd_array,
		       sizeof (SOCKET) * set.fd_count);
	}

	fd_set *GetPtr() { return &set; }
	int Size() { return set.fd_count; }
	bool IsEmpty() { return set.fd_count == 0; }
	bool IsFull() { return set.fd_count == FD_SETSIZE; }

	int operator[](int index) {
		assert(index >= 0 && (u_int)index < set.fd_count);
		return set.fd_array[index];
	}

	int Add(int fd) {
		assert(!IsFull());
		set.fd_array[set.fd_count] = fd;
		return set.fd_count++;
	}

	void MoveToEnd(int index) {
		assert(index >= 0 && (u_int)index < set.fd_count);
		std::swap(set.fd_array[index], set.fd_array[set.fd_count - 1]);
	}

	void RemoveLast() {
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

	bool CanModify(Item &item, unsigned events, int event_id);
	void Modify(Item &item, int fd, unsigned events, int event_id);

	PollGroupWinSelect(PollGroupWinSelect &) = delete;
	PollGroupWinSelect &operator=(PollGroupWinSelect &) = delete;
public:
	static constexpr unsigned READ = 1;
	static constexpr unsigned WRITE = 2;
	static constexpr unsigned ERROR = 0;
	static constexpr unsigned HANGUP = 0;

	PollGroupWinSelect();
	~PollGroupWinSelect();

	void ReadEvents(PollResultGeneric &result, int timeout_ms);
	bool Add(int fd, unsigned events, void *obj);
	bool Modify(int fd, unsigned events, void *obj);
	bool Remove(int fd);
	bool Abandon(int fd) { return Remove(fd); }
};

#endif
