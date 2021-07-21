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

#ifndef EVENT_WINSELECT_BACKEND_HXX
#define EVENT_WINSELECT_BACKEND_HXX

#include "PollResultGeneric.hxx"

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include <winsock2.h>

class SocketSet
{
	fd_set set;
public:
	SocketSet() noexcept {
		set.fd_count = 0;
	}

	SocketSet(const SocketSet &other) noexcept {
		set.fd_count = other.set.fd_count;
		std::copy_n(other.set.fd_array, set.fd_count, set.fd_array);
	}

	fd_set *GetPtr() noexcept {
		return IsEmpty() ? nullptr : &set;
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

	SOCKET operator[](size_t index) const noexcept {
		assert(index < set.fd_count);
		return set.fd_array[index];
	}

	size_t Add(SOCKET fd) noexcept {
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

	const auto *begin() const noexcept {
		return set.fd_array;
	}

	const auto *end() const noexcept {
		return set.fd_array + set.fd_count;
	}
};

class WinSelectBackend
{
	struct Item
	{
		int index[2]{-1, -1};
		void *obj;
		unsigned events = 0;

		explicit constexpr Item(void *_obj) noexcept
			:obj(_obj) {}

		Item(const Item &) = delete;
		Item &operator=(const Item &) = delete;
	};

	SocketSet event_set[2];
	std::unordered_map<SOCKET, Item> items;

public:
	WinSelectBackend() noexcept;
	~WinSelectBackend() noexcept;

	WinSelectBackend(const WinSelectBackend &) = delete;
	WinSelectBackend &operator=(const WinSelectBackend &) = delete;

	PollResultGeneric ReadEvents(int timeout_ms) noexcept;
	bool Add(SOCKET fd, unsigned events, void *obj) noexcept;
	bool Modify(SOCKET fd, unsigned events, void *obj) noexcept;
	bool Remove(SOCKET fd) noexcept;
	bool Abandon(SOCKET fd) noexcept {
		return Remove(fd);
	}

private:
	bool CanModify(Item &item, unsigned events,
		       int event_id) const noexcept;
	void Modify(Item &item, SOCKET fd, unsigned events,
		    int event_id) noexcept;

	void ApplyReady(const SocketSet &src, unsigned events) noexcept;
};

#endif
