/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#include "DatabaseHelpers.hxx"
#include "DatabasePlugin.hxx"
#include "Song.hxx"
#include "Tag.hxx"

#include <functional>
#include <set>

#include <string.h>

struct StringLess {
	gcc_pure
	bool operator()(const char *a, const char *b) const {
		return strcmp(a, b) < 0;
	}
};

typedef std::set<const char *, StringLess> StringSet;

static bool
CollectTags(StringSet &set, enum tag_type tag_type, Song &song)
{
	Tag *tag = song.tag;
	if (tag == nullptr)
		return true;

	bool found = false;
	for (unsigned i = 0; i < tag->num_items; ++i) {
		if (tag->items[i]->type == tag_type) {
			set.insert(tag->items[i]->value);
			found = true;
		}
	}

	if (!found)
		set.insert("");

	return true;
}

bool
VisitUniqueTags(const Database &db, const DatabaseSelection &selection,
		enum tag_type tag_type,
		VisitString visit_string,
		Error &error)
{
	StringSet set;

	using namespace std::placeholders;
	const auto f = std::bind(CollectTags, std::ref(set), tag_type, _1);
	if (!db.Visit(selection, f, error))
		return false;

	for (auto value : set)
		if (!visit_string(value, error))
			return false;

	return true;
}

static void
StatsVisitTag(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	      const Tag &tag)
{
	if (tag.time > 0)
		stats.total_duration += tag.time;

	for (unsigned i = 0; i < tag.num_items; ++i) {
		const TagItem &item = *tag.items[i];

		switch (item.type) {
		case TAG_ARTIST:
			artists.insert(item.value);
			break;

		case TAG_ALBUM:
			albums.insert(item.value);
			break;

		default:
			break;
		}
	}
}

static bool
StatsVisitSong(DatabaseStats &stats, StringSet &artists, StringSet &albums,
	       Song &song)
{
	++stats.song_count;

	if (song.tag != nullptr)
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
