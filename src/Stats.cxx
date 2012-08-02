/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

extern "C" {
#include "stats.h"
#include "database.h"
#include "db_selection.h"
#include "tag.h"
#include "song.h"
#include "client.h"
#include "player_control.h"
#include "strset.h"
#include "client_internal.h"
}

#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

#include <functional>
#include <set>

struct stats stats;

void stats_global_init(void)
{
	stats.timer = g_timer_new();
}

void stats_global_finish(void)
{
	g_timer_destroy(stats.timer);
}

struct StringLess {
	gcc_pure
	bool operator()(const char *a, const char *b) const {
		return strcmp(a, b) < 0;
	}
};

typedef std::set<const char *, StringLess> StringSet;

static void
visit_tag(StringSet &artists, StringSet &albums, const struct tag *tag)
{
	if (tag->time > 0)
		stats.song_duration += tag->time;

	for (unsigned i = 0; i < tag->num_items; ++i) {
		const struct tag_item *item = tag->items[i];

		switch (item->type) {
		case TAG_ARTIST:
			artists.insert(item->value);
			break;

		case TAG_ALBUM:
			albums.insert(item->value);
			break;

		default:
			break;
		}
	}
}

static bool
collect_stats_song(StringSet &artists, StringSet &albums, struct song *song)
{
	++stats.song_count;

	if (song->tag != NULL)
		visit_tag(artists, albums, song->tag);

	return true;
}

void stats_update(void)
{
	stats.song_count = 0;
	stats.song_duration = 0;
	stats.artist_count = 0;

	struct db_selection selection;
	db_selection_init(&selection, "", true);

	StringSet artists, albums;
	using namespace std::placeholders;
	const auto f = std::bind(collect_stats_song,
				 std::ref(artists), std::ref(albums), _1);
	GetDatabase()->Visit(&selection, f, NULL);

	stats.artist_count = artists.size();
	stats.album_count = albums.size();
}

int stats_print(struct client *client)
{
	client_printf(client,
		      "artists: %u\n"
		      "albums: %u\n"
		      "songs: %i\n"
		      "uptime: %li\n"
		      "playtime: %li\n"
		      "db_playtime: %li\n"
		      "db_update: %li\n",
		      stats.artist_count,
		      stats.album_count,
		      stats.song_count,
		      (long)g_timer_elapsed(stats.timer, NULL),
		      (long)(pc_get_total_play_time(client->player_control) + 0.5),
		      stats.song_duration,
		      (long)db_get_mtime());
	return 0;
}
