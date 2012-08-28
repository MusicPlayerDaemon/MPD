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

#include "config.h"

extern "C" {
#include "stats.h"
#include "database.h"
#include "client.h"
#include "player_control.h"
#include "client_internal.h"
}

#include "DatabaseSelection.hxx"
#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

struct stats stats;

void stats_global_init(void)
{
	stats.timer = g_timer_new();
}

void stats_global_finish(void)
{
	g_timer_destroy(stats.timer);
}

void stats_update(void)
{
	GError *error = nullptr;

	DatabaseStats stats2;

	const DatabaseSelection selection("", true);
	if (GetDatabase()->GetStats(selection, stats2, &error)) {
		stats.song_count = stats2.song_count;
		stats.song_duration = stats2.total_duration;
		stats.artist_count = stats2.artist_count;
		stats.album_count = stats2.album_count;
	} else {
		g_warning("%s", error->message);
		g_error_free(error);

		stats.song_count = 0;
		stats.song_duration = 0;
		stats.artist_count = 0;
		stats.album_count = 0;
	}
}

void
stats_print(struct client *client)
{
	client_printf(client,
		      "artists: %u\n"
		      "albums: %u\n"
		      "songs: %i\n"
		      "uptime: %li\n"
		      "playtime: %li\n"
		      "db_playtime: %li\n",
		      stats.artist_count,
		      stats.album_count,
		      stats.song_count,
		      (long)g_timer_elapsed(stats.timer, NULL),
		      (long)(pc_get_total_play_time(client->player_control) + 0.5),
		      stats.song_duration);

	if (db_is_simple())
		client_printf(client,
			      "db_update: %li\n",
			      (long)db_get_mtime());
}
