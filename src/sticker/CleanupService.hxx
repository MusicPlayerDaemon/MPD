// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Database.hxx"
#include "event/InjectEvent.hxx"
#include "thread/Thread.hxx"

#include <atomic>

class Database;
struct Instance;

/**
 * Delete stickers that no longer match items in the music database.
 *
 * When done calls Instance::OnSickerCleanupDone() in the instance event loop.
 */
class StickerCleanupService {
	/**
	 * number of stickers to delete in one transaction
	 */
	static constexpr std::size_t DeleteBatchSize = 50;

	Instance &instance;
	StickerDatabase sticker_db;
	Database &music_db;
	Thread thread{BIND_THIS_METHOD(Task)};
	InjectEvent defer;
	std::size_t deleted_count{0};
	std::atomic_bool cancel_flag{false};

public:
	StickerCleanupService(Instance &_instance,
			      StickerDatabase &_sticker_db,
			      Database &_db) noexcept;

	~StickerCleanupService() noexcept;

	auto &GetEventLoop() const noexcept {
		return defer.GetEventLoop();
	}

	void Start();

private:
	void Task() noexcept;

	void RunDeferred() noexcept;

	void CancelAndJoin() noexcept;
};
