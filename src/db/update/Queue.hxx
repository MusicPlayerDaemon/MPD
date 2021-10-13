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
