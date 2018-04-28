/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "Helpers.hxx"
#include "Stats.hxx"
#include "Interface.hxx"
#include "LightSong.hxx"
#include "tag/Tag.hxx"
#include "db/Selection.hxx"

#include <set>
#include <assert.h>
#include <string.h>

struct StringLess {
	gcc_pure
	bool operator()(const char *a, const char *b) const noexcept {
		return strcmp(a, b) < 0;
	}
};

typedef std::set<const char *, StringLess> StringSet;

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

	StatsVisitTag(stats, artists, albums, *song.tag);
}


static bool
StatsVisitUniqueSong(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	       StringSet &songs, const LightSong &song)
{
	assert(song.real_uri != nullptr);

#if CLANG_OR_GCC_VERSION(4,8)
	songs.emplace(song.real_uri);
#else
	songs.insert(song.real_uri);
#endif
	++stats.song_count;

	StatsVisitTag(stats, artists, albums, *song.tag);

	return true;
}

DatabaseStats
GetStats(const Database &db, const DatabaseSelection &selection)
{
	DatabaseStats stats;
	stats.Clear();

	StringSet artists, albums;
	using namespace std::placeholders;
	if (selection.ignore_repeat) {
		StringSet songs;
		const auto f = std::bind(StatsVisitUniqueSong,
					 std::ref(stats), std::ref(artists),
					 std::ref(albums), std::ref(songs), _1);
		db.Visit(selection, f);
		stats.song_count = songs.size();
	} else {
		const auto f = std::bind(StatsVisitSong,
					 std::ref(stats), std::ref(artists),
					 std::ref(albums), _1);
		db.Visit(selection, f);
	}
	stats.artist_count = artists.size();
	stats.album_count = albums.size();
	return stats;
}
