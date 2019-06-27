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

#ifdef USE_WINSELECT

#include "PollGroupWinSelect.hxx"

constexpr int EVENT_READ = 0;
constexpr int EVENT_WRITE = 1;

static constexpr
bool HasEvent(unsigned events, int event_id) noexcept
{
	return (events & (1 << event_id)) != 0;
}

PollGroupWinSelect::PollGroupWinSelect() noexcept = default;
PollGroupWinSelect::~PollGroupWinSelect() noexcept = default;

bool
PollGroupWinSelect::CanModify(PollGroupWinSelect::Item &item,
			      unsigned events, int event_id) const noexcept
{
	if (item.index[event_id] < 0 && HasEvent(events, event_id))
		return !event_set[event_id].IsFull();
	return true;
}

void
PollGroupWinSelect::Modify(PollGroupWinSelect::Item &item, int fd,
			   unsigned events, int event_id) noexcept
{
	int index = item.index[event_id];
	auto &set = event_set[event_id];

	if (index < 0 && HasEvent(events, event_id))
		item.index[event_id] = set.Add(fd);
	else if (index >= 0 && !HasEvent(events, event_id)) {
		if (size_t(index) != set.Size() - 1) {
			set.MoveToEnd(index);
			items[set[index]].index[event_id] = index;
		}
		set.RemoveLast();
		item.index[event_id] = -1;
	}
}

bool
PollGroupWinSelect::Add(int fd, unsigned events, void *obj) noexcept
{
	assert(items.find(fd) == items.end());
	auto &item = items[fd];

	item.index[EVENT_READ] = -1;
	item.index[EVENT_WRITE] = -1;
	item.obj = obj;
	item.events = 0;

	if (!CanModify(item, events, EVENT_READ)) {
		items.erase(fd);
		return false;
	}
	if (!CanModify(item, events, EVENT_WRITE)) {
		items.erase(fd);
		return false;
	}

	Modify(item, fd, events, EVENT_READ);
	Modify(item, fd, events, EVENT_WRITE);
	return true;
}

bool
PollGroupWinSelect::Modify(int fd, unsigned events, void *obj) noexcept
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
PollGroupWinSelect::Remove(int fd) noexcept
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
PollGroupWinSelect::ReadEvents(PollResultGeneric &result,
			       int timeout_ms) noexcept
{
	bool use_sleep = event_set[EVENT_READ].IsEmpty() &&
			 event_set[EVENT_WRITE].IsEmpty();

	if (use_sleep) {
		Sleep(timeout_ms < 0 ? INFINITE : (DWORD) timeout_ms);
		return;
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
			 read_set.IsEmpty() ? nullptr : read_set.GetPtr(),
			 write_set.IsEmpty() ? nullptr : write_set.GetPtr(),
			 except_set.IsEmpty() ? nullptr : except_set.GetPtr(),
			 timeout_ms < 0 ? nullptr : &tv);

	if (ret == 0 || ret == SOCKET_ERROR)
		return;

	for (size_t i = 0; i < read_set.Size(); ++i)
		items[read_set[i]].events |= READ;

	for (size_t i = 0; i < write_set.Size(); ++i)
		items[write_set[i]].events |= WRITE;

	for (size_t i = 0; i < except_set.Size(); ++i)
		items[except_set[i]].events |= WRITE;

	for (auto i = items.begin(); i != items.end(); ++i)
		if (i->second.events != 0) {
			result.Add(i->second.events, i->second.obj);
			i->second.events = 0;
		}
}

#endif
