// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
