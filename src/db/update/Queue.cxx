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

#include "config.h"
#include "Queue.hxx"

bool
UpdateQueue::Push(const char *path, bool discard, unsigned id)
{
	if (update_queue.size() >= MAX_UPDATE_QUEUE_SIZE)
		return false;

	update_queue.emplace(path, discard, id);
	return true;
}

UpdateQueueItem
UpdateQueue::Pop()
{
	if (update_queue.empty())
		return UpdateQueueItem();

	auto i = std::move(update_queue.front());
	update_queue.pop();
	return i;
}
