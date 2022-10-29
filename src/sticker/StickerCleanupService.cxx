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

#include "StickerCleanupService.hxx"

#include "Sticker.hxx"
#include "Database.hxx"
#include "song/Filter.hxx"
#include "TagSticker.hxx"
#include "Log.hxx"
#include "Instance.hxx"
#include "thread/Name.hxx"

StickerCleanupService::StickerCleanupService(Instance &_instance,
					     StickerDatabase &_sticker_db,
					     Database &_db) noexcept:
	instance(_instance),
	sticker_db(_sticker_db),
	music_db(_db),
	thread(BIND_THIS_METHOD(Task)),
	defer(_instance.event_loop, BIND_THIS_METHOD(RunDeferred)) {

}

StickerCleanupService::~StickerCleanupService() noexcept
{
	// call only by the owning instance
	assert(instance.event_loop.IsInside());

	CancelAndJoin();
}

void
StickerCleanupService::Start()
{
	// call only by the owning instance
	assert(instance.event_loop.IsInside());

	thread.Start();

	FmtDebug(sticker_domain,
		 "spawned thread for cleanup job");
}

void
StickerCleanupService::RunDeferred() noexcept
{
	instance.OnSickerCleanupDone(this, deleted_count != 0);
}

namespace {
inline
std::size_t
DeleteStickers(StickerDatabase &sticker_db, std::list<StickerDatabase::StickerTypeUriPair> &stickers)
{
	if (stickers.empty())
		return 0;
	sticker_db.BatchDeleteNoIdle(stickers);
	auto count = stickers.size();
	stickers.clear();
	return count;
}

}//namespace

void
StickerCleanupService::Task() noexcept
{
	SetThreadName("sticker");

	FmtDebug(sticker_domain, "begin cleanup");

	try {
		auto stickers = sticker_db.GetUniqueStickers();
		auto iter = stickers.cbegin();
		auto batch = std::list<StickerDatabase::StickerTypeUriPair>();
		while (!cancel_flag && !stickers.empty()) {

			const auto &sticker_type = iter->first;
			const auto &sticker_uri = iter->second;

			const auto filter = MakeSongFilterNoThrow(sticker_type, sticker_uri);

			if (filter.IsEmpty() || FilterMatches(music_db, filter))
				// skip if found a match or if not a valid sticker filter
				iter = stickers.erase(iter);
			else {
				batch.splice(batch.end(), stickers, iter++);
				if (batch.size() == DeleteBatchSize)
					deleted_count += DeleteStickers(sticker_db, batch);
			}
		}
		if (!cancel_flag)
			deleted_count += DeleteStickers(sticker_db, batch);
	}
	catch (std::runtime_error &e) {
		FmtError(sticker_domain, "cleanup failed: {}", e.what());
	}

	defer.Schedule();

	FmtDebug(sticker_domain, "end cleanup: {} stickers deleted", deleted_count);
}

void
StickerCleanupService::CancelAndJoin() noexcept
{
	if (thread.IsDefined()) {
		cancel_flag = true;
		thread.Join();
	}
}

