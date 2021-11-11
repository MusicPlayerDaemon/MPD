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

#include "Queue.hxx"

bool
UpdateQueue::Push(SimpleDatabase &db, Storage &storage,
		  std::string_view path, bool discard, unsigned id) noexcept
{
	if (update_queue.size() >= MAX_UPDATE_QUEUE_SIZE)
		return false;

	update_queue.emplace_back(db, storage, path, discard, id);
	return true;
}

UpdateQueueItem
UpdateQueue::Pop() noexcept
{
	if (update_queue.empty())
		return {};

	auto i = std::move(update_queue.front());
	update_queue.pop_front();
	return i;
}

void
UpdateQueue::Erase(SimpleDatabase &db) noexcept
{
	for (auto i = update_queue.begin(), end = update_queue.end();
	     i != end;) {
		if (i->db == &db)
			i = update_queue.erase(i);
		else
			++i;
	}
}

void
UpdateQueue::Erase(Storage &storage) noexcept
{
	for (auto i = update_queue.begin(), end = update_queue.end();
	     i != end;) {
		if (i->storage == &storage)
			i = update_queue.erase(i);
		else
			++i;
	}
}
