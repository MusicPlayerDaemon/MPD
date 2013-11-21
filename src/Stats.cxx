/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Stats.hxx"
#include "PlayerControl.hxx"
#include "Client.hxx"
#include "DatabaseSelection.hxx"
#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"
#include "DatabaseSimple.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <glib.h>

static GTimer *uptime;
static DatabaseStats stats;

void stats_global_init(void)
{
	uptime = g_timer_new();
}

void stats_global_finish(void)
{
	g_timer_destroy(uptime);
}

void stats_update(void)
{
	Error error;

	DatabaseStats stats2;

	const DatabaseSelection selection("", true);
	if (GetDatabase()->GetStats(selection, stats2, error)) {
		stats = stats2;
	} else {
		LogError(error);

		stats.Clear();
	}
}

void
stats_print(Client &client)
{
	if (!db_is_simple())
		/* reload statistics if we're using the "proxy"
		   database plugin */
		/* TODO: move this into the "proxy" database plugin as
		   an "idle" handler */
		stats_update();

	client_printf(client,
		      "artists: %u\n"
		      "albums: %u\n"
		      "songs: %u\n"
		      "uptime: %lu\n"
		      "playtime: %lu\n"
		      "db_playtime: %lu\n",
		      stats.artist_count,
		      stats.album_count,
		      stats.song_count,
		      (unsigned long)g_timer_elapsed(uptime, NULL),
		      (unsigned long)(client.player_control.GetTotalPlayTime() + 0.5),
		      stats.total_duration);

	if (db_is_simple())
		client_printf(client,
			      "db_update: %lu\n",
			      (unsigned long)db_get_mtime());
}
