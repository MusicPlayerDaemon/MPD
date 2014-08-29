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
#include "Stats.hxx"
#include "PlayerControl.hxx"
#include "client/Client.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "db/Selection.hxx"
#include "db/Interface.hxx"
#include "db/Stats.hxx"
#include "util/Error.hxx"
#include "system/Clock.hxx"
#include "Log.hxx"

#ifndef WIN32
/**
 * The monotonic time stamp when MPD was started.  It is used to
 * calculate the uptime.
 */
static unsigned start_time;
#endif

#ifdef ENABLE_DATABASE

static DatabaseStats stats;

enum class StatsValidity : uint8_t {
	INVALID, VALID, FAILED,
};

static StatsValidity stats_validity = StatsValidity::INVALID;

#endif

void stats_global_init(void)
{
#ifndef WIN32
	start_time = MonotonicClockS();
#endif
}

#ifdef ENABLE_DATABASE

void
stats_invalidate()
{
	stats_validity = StatsValidity::INVALID;
}

static bool
stats_update(const Database &db)
{
	switch (stats_validity) {
	case StatsValidity::INVALID:
		break;

	case StatsValidity::VALID:
		return true;

	case StatsValidity::FAILED:
		return false;
	}

	Error error;

	const DatabaseSelection selection("", true);
	if (db.GetStats(selection, stats, error)) {
		stats_validity = StatsValidity::VALID;
		return true;
	} else {
		LogError(error);

		stats_validity = StatsValidity::FAILED;
		return false;
	}
}

static void
db_stats_print(Client &client, const Database &db)
{
	if (!stats_update(db))
		return;

	unsigned total_duration_s =
		std::chrono::duration_cast<std::chrono::seconds>(stats.total_duration).count();

	client_printf(client,
		      "artists: %u\n"
		      "albums: %u\n"
		      "songs: %u\n"
		      "db_playtime: %u\n",
		      stats.artist_count,
		      stats.album_count,
		      stats.song_count,
		      total_duration_s);

	const time_t update_stamp = db.GetUpdateStamp();
	if (update_stamp > 0)
		client_printf(client,
			      "db_update: %lu\n",
			      (unsigned long)update_stamp);
}

#endif

void
stats_print(Client &client)
{
	client_printf(client,
		      "uptime: %u\n"
		      "playtime: %lu\n",
#ifdef WIN32
		      GetProcessUptimeS(),
#else
		      MonotonicClockS() - start_time,
#endif
		      (unsigned long)(client.player_control.GetTotalPlayTime() + 0.5));

#ifdef ENABLE_DATABASE
	const Database *db = client.partition.instance.database;
	if (db != nullptr)
		db_stats_print(client, *db);
#endif
}
