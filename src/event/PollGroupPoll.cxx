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

#include "config.h"

#ifdef USE_POLL

#include "PollGroupPoll.hxx"

#include <assert.h>

PollGroupPoll::PollGroupPoll() noexcept = default;
PollGroupPoll::~PollGroupPoll() noexcept = default;

bool
PollGroupPoll::Add(int fd, unsigned events, void *obj) noexcept
{
	assert(items.find(fd) == items.end());

	const size_t index = poll_events.size();
	poll_events.resize(index + 1);
	auto &e = poll_events[index];
	e.fd = fd;
	e.events = events;
	e.revents = 0;
	auto &item = items[fd];
	item.index = index;
	item.obj = obj;
	return true;
}

bool
PollGroupPoll::Modify(int fd, unsigned events, void *obj) noexcept
{
	auto item_iter = items.find(fd);
	assert(item_iter != items.end());
	auto &item = item_iter->second;
	item.obj = obj;
	auto &e = poll_events[item.index];
	e.events = events;
	e.revents &= events;
	return true;
}

bool
PollGroupPoll::Remove(int fd) noexcept
{
	auto item_iter = items.find(fd);
	assert(item_iter != items.end());
	auto &item = item_iter->second;
	size_t index = item.index;
	size_t last_index = poll_events.size() - 1;
	if (index != last_index) {
		std::swap(poll_events[index], poll_events[last_index]);
		items[poll_events[index].fd].index = index;
	}
	poll_events.pop_back();
	items.erase(item_iter);
	return true;
}

void
PollGroupPoll::ReadEvents(PollResultGeneric &result, int timeout_ms) noexcept
{
	int n = poll(poll_events.empty() ? nullptr : &poll_events[0],
		     poll_events.size(), timeout_ms);

	for (size_t i = 0; n > 0 && i < poll_events.size(); ++i) {
		const auto &e = poll_events[i];
		if (e.revents != 0) {
			result.Add(e.revents, items[e.fd].obj);
			--n;
		}
	}
}

#endif
