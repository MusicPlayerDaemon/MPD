/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#ifndef CLEAUPTHREAD_HXX
#define CLEAUPTHREAD_HXX

#include "event/InjectEvent.hxx"
#include "thread/Thread.hxx"

#include <atomic>

class Database;
class StickerDatabase;
struct Instance;

/**
 * Delete stickers that no longer match items in the music database.
 * <br/>
 * When done calls Instance::OnSickerCleanupDone() in the instance event loop.
 */
class StickerCleanupService {
public:
	StickerCleanupService(Instance &_instance,
			      StickerDatabase &_sticker_db,
			      Database &_db) noexcept;

	~StickerCleanupService() noexcept;

	void Start();

private:
	void Task() noexcept;

	void RunDeferred() noexcept;

	void CancelAndJoin() noexcept;

	/**
	 * number of stickers to delete in one transaction
	 */
	static constexpr std::size_t DeleteBatchSize = 50;

	Instance &instance;
	StickerDatabase &sticker_db;
	Database &music_db;
	Thread thread;
	InjectEvent defer;
	std::size_t deleted_count{0};
	std::atomic<bool> cancel_flag{false};
};


#endif // CLEAUPTHREAD_HXX
