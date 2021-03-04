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

#include "WinSelectBackend.hxx"
#include "WinSelectEvents.hxx"

static constexpr int EVENT_READ = 0;
static constexpr int EVENT_WRITE = 1;

static constexpr
bool HasEvent(unsigned events, int event_id) noexcept
{
	return (events & (1 << event_id)) != 0;
}

WinSelectBackend::WinSelectBackend() noexcept = default;
WinSelectBackend::~WinSelectBackend() noexcept = default;

bool
WinSelectBackend::CanModify(WinSelectBackend::Item &item,
			    unsigned events, int event_id) const noexcept
{
	if (item.index[event_id] < 0 && HasEvent(events, event_id))
		return !event_set[event_id].IsFull();
	return true;
}

void
WinSelectBackend::Modify(WinSelectBackend::Item &item, SOCKET fd,
			 unsigned events, int event_id) noexcept
{
	int index = item.index[event_id];
	auto &set = event_set[event_id];

	if (index < 0 && HasEvent(events, event_id))
		item.index[event_id] = set.Add(fd);
	else if (index >= 0 && !HasEvent(events, event_id)) {
		if (size_t(index) != set.Size() - 1) {
			set.MoveToEnd(index);
			items.find(set[index])->second.index[event_id] = index;
		}
		set.RemoveLast();
		item.index[event_id] = -1;
	}
}

bool
WinSelectBackend::Add(SOCKET fd, unsigned events, void *obj) noexcept
{
	assert(items.find(fd) == items.end());
	auto i = items.emplace(std::piecewise_construct,
			       std::forward_as_tuple(fd),
			       std::forward_as_tuple(obj)).first;
	auto &item = i->second;

	if (!CanModify(item, events, EVENT_READ)) {
		items.erase(i);
		return false;
	}
	if (!CanModify(item, events, EVENT_WRITE)) {
		items.erase(i);
		return false;
	}

	Modify(item, fd, events, EVENT_READ);
	Modify(item, fd, events, EVENT_WRITE);
	return true;
}

bool
WinSelectBackend::Modify(SOCKET fd, unsigned events, void *obj) noexcept
{
	auto item_iter = items.find(fd);
	assert(item_iter != items.end());
	auto &item = item_iter->second;

	if (!CanModify(item, events, EVENT_READ))
		return false;
	if (!CanModify(item, events, EVENT_WRITE))
		return false;

	item.obj = obj;
	Modify(item, fd, events, EVENT_READ);
	Modify(item, fd, events, EVENT_WRITE);
	return true;
}

bool
WinSelectBackend::Remove(SOCKET fd) noexcept
{
	auto item_iter = items.find(fd);
	assert(item_iter != items.end());
	auto &item = item_iter->second;

	Modify(item, fd, 0, EVENT_READ);
	Modify(item, fd, 0, EVENT_WRITE);
	items.erase(item_iter);
	return true;
}

void
WinSelectBackend::ApplyReady(const SocketSet &src, unsigned events) noexcept
{
	for (const auto i : src) {
		auto it = items.find(i);
		assert(it != items.end());

		it->second.events |= events;
	}
}

PollResultGeneric
WinSelectBackend::ReadEvents(int timeout_ms) noexcept
{
	bool use_sleep = event_set[EVENT_READ].IsEmpty() &&
			 event_set[EVENT_WRITE].IsEmpty();

	PollResultGeneric result;
	if (use_sleep) {
		Sleep(timeout_ms < 0 ? INFINITE : (DWORD) timeout_ms);
		return result;
	}

	SocketSet read_set(event_set[EVENT_READ]);
	SocketSet write_set(event_set[EVENT_WRITE]);
	SocketSet except_set(event_set[EVENT_WRITE]);

	timeval tv;
	if (timeout_ms >= 0) {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
	}

	int ret = select(0,
			 read_set.GetPtr(),
			 write_set.GetPtr(),
			 except_set.GetPtr(),
			 timeout_ms < 0 ? nullptr : &tv);

	if (ret == 0 || ret == SOCKET_ERROR)
		return result;

	ApplyReady(read_set, WinSelectEvents::READ);
	ApplyReady(write_set, WinSelectEvents::WRITE);
	ApplyReady(except_set, WinSelectEvents::WRITE);

	for (auto &[key, item] : items)
		if (item.events != 0) {
			result.Add(item.events, item.obj);
			item.events = 0;
		}

	return result;
}
