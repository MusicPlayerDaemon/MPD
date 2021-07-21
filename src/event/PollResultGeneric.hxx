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

#ifndef MPD_EVENT_POLLRESULT_GENERIC_HXX
#define MPD_EVENT_POLLRESULT_GENERIC_HXX

#include <cstddef>
#include <vector>

#ifdef _WIN32
#include <windows.h>
/* damn you, windows.h! */
#ifdef GetObject
#undef GetObject
#endif
#endif

class PollResultGeneric
{
	struct Item
	{
		unsigned events;
		void *obj;

		Item() = default;
		constexpr Item(unsigned _events, void *_obj) noexcept
			: events(_events), obj(_obj) { }
	};

	std::vector<Item> items;
public:
	size_t GetSize() const noexcept {
		return items.size();
	}

	unsigned GetEvents(size_t i) const noexcept {
		return items[i].events;
	}

	void *GetObject(size_t i) const noexcept {
		return items[i].obj;
	}

	void Add(unsigned events, void *obj) noexcept {
		items.emplace_back(events, obj);
	}
};

#endif
