// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Helpers.hxx"
#include "Stats.hxx"
#include "Interface.hxx"
#include "song/LightSong.hxx"
#include "tag/Tag.hxx"

#include <set>

#include <string.h>

struct StringLess {
	[[gnu::pure]]
	bool operator()(const char *a, const char *b) const noexcept {
		return strcmp(a, b) < 0;
	}
};

using StringSet = std::set<const char *, StringLess>;

static void
StatsVisitTag(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	      const Tag &tag) noexcept
{
	if (!tag.duration.IsNegative())
		stats.total_duration += tag.duration;

	for (const auto &item : tag) {
		switch (item.type) {
		case TAG_ARTIST:
			artists.emplace(item.value);
			break;

		case TAG_ALBUM:
			albums.emplace(item.value);
			break;

		default:
			break;
		}
	}
}

static void
StatsVisitSong(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	       const LightSong &song) noexcept
{
	++stats.song_count;

	StatsVisitTag(stats, artists, albums, song.tag);
}

DatabaseStats
GetStats(const Database &db, const DatabaseSelection &selection)
{
	DatabaseStats stats;
	stats.Clear();

	StringSet artists, albums;
	const auto f = [&](const auto &song)
		{ return StatsVisitSong(stats, artists, albums, song); };

	db.Visit(selection, f);

	stats.artist_count = artists.size();
	stats.album_count = albums.size();
	return stats;
}
