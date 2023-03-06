// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
