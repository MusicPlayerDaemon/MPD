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

#include "Count.hxx"
#include "Selection.hxx"
#include "Interface.hxx"
#include "Partition.hxx"
#include "client/Response.hxx"
#include "song/LightSong.hxx"
#include "tag/Tag.hxx"
#include "tag/VisitFallback.hxx"
#include "TagPrint.hxx"

#include <fmt/format.h>

#include <functional>
#include <map>

struct SearchStats {
	unsigned n_songs{0};
	std::chrono::duration<std::uint64_t, SongTime::period> total_duration;

	constexpr SearchStats()
		: total_duration(0) {}
};

class TagCountMap : public std::map<std::string, SearchStats> {
};

static void
PrintSearchStats(Response &r, const SearchStats &stats) noexcept
{
	unsigned total_duration_s =
		std::chrono::duration_cast<std::chrono::seconds>(stats.total_duration).count();

	r.Fmt(FMT_STRING("songs: {}\n"
			 "playtime: {}\n"),
	      stats.n_songs, total_duration_s);
}

static void
Print(Response &r, TagType group, const TagCountMap &m) noexcept
{
	assert(unsigned(group) < TAG_NUM_OF_ITEM_TYPES);

	for (const auto &[tag, stats] : m) {
		tag_print(r, group, tag.c_str());
		PrintSearchStats(r, stats);
	}
}

static void
stats_visitor_song(SearchStats &stats, const LightSong &song) noexcept
{
	stats.n_songs++;

	if (const auto duration = song.GetDuration(); !duration.IsNegative())
		stats.total_duration += duration;
}

static void
CollectGroupCounts(TagCountMap &map, const Tag &tag,
		   const char *value) noexcept
{
	auto &s = map.emplace(value, SearchStats()).first->second;
	++s.n_songs;
	if (!tag.duration.IsNegative())
		s.total_duration += tag.duration;
}

static void
GroupCountVisitor(TagCountMap &map, TagType group,
		  const LightSong &song) noexcept
{
	const Tag &tag = song.tag;
	VisitTagWithFallbackOrEmpty(tag, group, [&](const auto &val)
		{ return CollectGroupCounts(map, tag, val);  });
}

void
PrintSongCount(Response &r, const Partition &partition, const char *name,
	       const SongFilter *filter,
	       TagType group)
{
	const Database &db = partition.GetDatabaseOrThrow();

	const DatabaseSelection selection(name, true, filter);

	if (group == TAG_NUM_OF_ITEM_TYPES) {
		/* no grouping */

		SearchStats stats;

		const auto f = [&](const auto &song)
			{ return stats_visitor_song(stats, song); };

		db.Visit(selection, f);

		PrintSearchStats(r, stats);
	} else {
		/* group by the specified tag: store counts in a
		   std::map */

		TagCountMap map;

		const auto f = [&map,group](const auto &song)
			{ return GroupCountVisitor(map, group, song); };

		db.Visit(selection, f);

		Print(r, group, map);
	}
}
