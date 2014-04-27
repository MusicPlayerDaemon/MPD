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

#include <functional>

struct SearchStats {
	unsigned n_songs;
	unsigned long total_time_s;

	constexpr SearchStats()
		:n_songs(0), total_time_s(0) {}
};

static void
PrintSearchStats(Client &client, const SearchStats &stats)
{
	client_printf(client,
		      "songs: %u\n"
		      "playtime: %lu\n",
		      stats.n_songs, stats.total_time_s);
}

static bool
stats_visitor_song(SearchStats &stats, const LightSong &song)
{
	stats.n_songs++;
	stats.total_time_s += song.GetDuration();

	return true;
}

bool
PrintSongCount(Client &client, const char *name,
	       const SongFilter *filter,
	       Error &error)
{
	const Database *db = client.GetDatabase(error);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection(name, true, filter);

	SearchStats stats;

	using namespace std::placeholders;
	const auto f = std::bind(stats_visitor_song, std::ref(stats),
				 _1);
	if (!db->Visit(selection, f, error))
		return false;

	PrintSearchStats(client, stats);
	return true;
}
