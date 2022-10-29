// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "CleanupService.hxx"
#include "Database.hxx"
#include "Sticker.hxx"
#include "TagSticker.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "song/Filter.hxx"
#include "thread/Name.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "Instance.hxx"

static constexpr Domain sticker_domain{"sticker"};

StickerCleanupService::StickerCleanupService(Instance &_instance,
					     StickerDatabase &_sticker_db,
					     Database &_db) noexcept
	:instance(_instance),
	 sticker_db(_sticker_db.Reopen()),
	 music_db(_db),
	 defer(_instance.event_loop, BIND_THIS_METHOD(RunDeferred))
{
}

StickerCleanupService::~StickerCleanupService() noexcept
{
	// call only by the owning instance
	assert(GetEventLoop().IsInside());

	CancelAndJoin();
}

void
StickerCleanupService::Start()
{
	// call only by the owning instance
	assert(GetEventLoop().IsInside());

	thread.Start();

	FmtDebug(sticker_domain,
		 "spawned thread for cleanup job");
}

void
StickerCleanupService::RunDeferred() noexcept
{
	instance.OnStickerCleanupDone(deleted_count != 0);
}

static std::size_t
DeleteStickers(StickerDatabase &sticker_db,
	       std::list<StickerDatabase::StickerTypeUriPair> &stickers)
{
	if (stickers.empty())
		return 0;
	sticker_db.BatchDeleteNoIdle(stickers);
	auto count = stickers.size();
	stickers.clear();
	return count;
}

void
StickerCleanupService::Task() noexcept
{
	SetThreadName("sticker");

	FmtDebug(sticker_domain, "begin cleanup");

	try {
		auto stickers = sticker_db.GetUniqueStickers();
		auto iter = stickers.cbegin();
		std::list<StickerDatabase::StickerTypeUriPair> batch;
		while (!cancel_flag && !stickers.empty()) {
			const auto &[sticker_type, sticker_uri] = *iter;

			const auto filter = MakeSongFilterNoThrow(sticker_type.c_str(), sticker_uri.c_str());

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
	} catch (...) {
		FmtError(sticker_domain, "cleanup failed: {}",
			 std::current_exception());
	}

	defer.Schedule();

	FmtDebug(sticker_domain, "end cleanup: {} stickers deleted",
		 deleted_count);
}

void
StickerCleanupService::CancelAndJoin() noexcept
{
	if (thread.IsDefined()) {
		cancel_flag = true;
		thread.Join();
	}
}

