// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_UPDATE_QUEUE_HXX
#define MPD_UPDATE_QUEUE_HXX

#include <string>
#include <string_view>
#include <list>

class SimpleDatabase;
class Storage;

struct UpdateQueueItem {
	SimpleDatabase *db;
	Storage *storage;

	std::string path_utf8;
	unsigned id;
	bool discard;

	UpdateQueueItem() noexcept:id(0) {}

	UpdateQueueItem(SimpleDatabase &_db,
			Storage &_storage,
			std::string_view _path, bool _discard,
			unsigned _id) noexcept
		:db(&_db), storage(&_storage), path_utf8(_path),
		 id(_id), discard(_discard) {}

	bool IsDefined() const noexcept {
		return id != 0;
	}

	void Clear() noexcept {
		id = 0;
	}
};

class UpdateQueue {
	static constexpr unsigned MAX_UPDATE_QUEUE_SIZE = 32;

	std::list<UpdateQueueItem> update_queue;

public:
	bool Push(SimpleDatabase &db, Storage &storage,
		  std::string_view path, bool discard, unsigned id) noexcept;

	UpdateQueueItem Pop() noexcept;

	void Clear() noexcept {
		update_queue.clear();
	}

	void Erase(SimpleDatabase &db) noexcept;

	void Erase(Storage &storage) noexcept;
};

#endif
