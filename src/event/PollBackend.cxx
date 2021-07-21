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

#include "PollBackend.hxx"

#include <cassert>

PollBackend::PollBackend() noexcept = default;
PollBackend::~PollBackend() noexcept = default;

static constexpr auto
MakePollfd(int fd, short events) noexcept
{
	struct pollfd pfd{};
	pfd.fd = fd;
	pfd.events = events;
	return pfd;
}

bool
PollBackend::Add(int fd, unsigned events, void *obj) noexcept
{
	assert(items.find(fd) == items.end());

	const std::size_t index = poll_events.size();
	poll_events.push_back(MakePollfd(fd, events));

	items.emplace(std::piecewise_construct,
		      std::forward_as_tuple(fd),
		      std::forward_as_tuple(index, obj));
	return true;
}

bool
PollBackend::Modify(int fd, unsigned events, void *obj) noexcept
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
PollBackend::Remove(int fd) noexcept
{
	auto item_iter = items.find(fd);
	assert(item_iter != items.end());
	const auto &item = item_iter->second;
	std::size_t index = item.index;
	std::size_t last_index = poll_events.size() - 1;
	if (index != last_index) {
		std::swap(poll_events[index], poll_events[last_index]);
		items.find(poll_events[index].fd)->second.index = index;
	}
	poll_events.pop_back();
	items.erase(item_iter);
	return true;
}

PollResultGeneric
PollBackend::ReadEvents(int timeout_ms) noexcept
{
	int n = poll(poll_events.empty() ? nullptr : &poll_events[0],
		     poll_events.size(), timeout_ms);

	PollResultGeneric result;
	for (std::size_t i = 0; n > 0 && i < poll_events.size(); ++i) {
		const auto &e = poll_events[i];
		if (e.revents != 0) {
			auto it = items.find(e.fd);
			assert(it != items.end());
			assert(it->second.index == i);

			result.Add(e.revents, it->second.obj);
			--n;
		}
	}

	return result;
}
