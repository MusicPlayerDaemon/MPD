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

#include "Helpers.hxx"
#include "Stats.hxx"
#include "Interface.hxx"
#include "LightSong.hxx"
#include "tag/Tag.hxx"

#include <set>

#include <string.h>

struct StringLess {
	gcc_pure
	bool operator()(const char *a, const char *b) const {
		return strcmp(a, b) < 0;
	}
};

typedef std::set<const char *, StringLess> StringSet;

static void
StatsVisitTag(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	      const Tag &tag)
{
	if (!tag.duration.IsNegative())
		stats.total_duration += tag.duration;

	for (const auto &item : tag) {
		switch (item.type) {
		case TAG_ARTIST:
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
			artists.emplace(item.value);
#else
			artists.insert(item.value);
#endif
			break;

		case TAG_ALBUM:
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
			albums.emplace(item.value);
#else
			albums.insert(item.value);
#endif
			break;

		default:
			break;
		}
	}
}

static bool
StatsVisitSong(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	       const LightSong &song)
{
	++stats.song_count;

	StatsVisitTag(stats, artists, albums, *song.tag);

	return true;
}

bool
GetStats(const Database &db, const DatabaseSelection &selection,
	 DatabaseStats &stats, Error &error)
{
	stats.Clear();

	StringSet artists, albums;
	using namespace std::placeholders;
	const auto f = std::bind(StatsVisitSong,
				 std::ref(stats), std::ref(artists),
				 std::ref(albums), _1);
	if (!db.Visit(selection, f, error))
		return false;

	stats.artist_count = artists.size();
	stats.album_count = albums.size();
	return true;
}
