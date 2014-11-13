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

#include "config.h"
#include "Count.hxx"
#include "Selection.hxx"
#include "Interface.hxx"
#include "client/Client.hxx"
#include "LightSong.hxx"
#include "tag/Tag.hxx"

#include <functional>
#include <map>

struct SearchStats {
	unsigned n_songs;
	std::chrono::duration<std::uint64_t, SongTime::period> total_duration;

	constexpr SearchStats()
		:n_songs(0), total_duration(0) {}
};

class TagCountMap : public std::map<std::string, SearchStats> {
};

static void
PrintSearchStats(Client &client, const SearchStats &stats)
{
	unsigned total_duration_s =
		std::chrono::duration_cast<std::chrono::seconds>(stats.total_duration).count();

	client_printf(client,
		      "songs: %u\n"
		      "playtime: %u\n",
		      stats.n_songs, total_duration_s);
}

static void
Print(Client &client, TagType group, const TagCountMap &m)
{
	assert(unsigned(group) < TAG_NUM_OF_ITEM_TYPES);

	for (const auto &i : m) {
		client_printf(client, "%s: %s\n",
			      tag_item_names[group], i.first.c_str());
		PrintSearchStats(client, i.second);
	}
}

static bool
stats_visitor_song(SearchStats &stats, const LightSong &song)
{
	stats.n_songs++;

	const auto duration = song.GetDuration();
	if (!duration.IsNegative())
		stats.total_duration += duration;

	return true;
}

static bool
CollectGroupCounts(TagCountMap &map, TagType group, const Tag &tag)
{
	bool found = false;
	for (const auto &item : tag) {
		if (item.type == group) {
			auto r = map.insert(std::make_pair(item.value,
							   SearchStats()));
			SearchStats &s = r.first->second;
			++s.n_songs;
			if (!tag.duration.IsNegative())
				s.total_duration += tag.duration;

			found = true;
		}
	}

	return found;
}

static bool
GroupCountVisitor(TagCountMap &map, TagType group, const LightSong &song)
{
	assert(song.tag != nullptr);

	const Tag &tag = *song.tag;
	if (!CollectGroupCounts(map, group, tag) && group == TAG_ALBUM_ARTIST)
		/* fall back to "Artist" if no "AlbumArtist" was found */
		CollectGroupCounts(map, TAG_ARTIST, tag);

	return true;
}

bool
PrintSongCount(Client &client, const char *name,
	       const SongFilter *filter,
	       TagType group,
	       Error &error)
{
	const Database *db = client.GetDatabase(error);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection(name, true, filter);

	if (group == TAG_NUM_OF_ITEM_TYPES) {
		/* no grouping */

		SearchStats stats;

		using namespace std::placeholders;
		const auto f = std::bind(stats_visitor_song, std::ref(stats),
					 _1);
		if (!db->Visit(selection, f, error))
			return false;

		PrintSearchStats(client, stats);
	} else {
		/* group by the specified tag: store counts in a
		   std::map */

		TagCountMap map;

		using namespace std::placeholders;
		const auto f = std::bind(GroupCountVisitor, std::ref(map),
					 group, _1);
		if (!db->Visit(selection, f, error))
			return false;

		Print(client, group, map);
	}

	return true;
}
