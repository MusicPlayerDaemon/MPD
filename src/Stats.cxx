// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "Stats.hxx"
#include "player/Control.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "db/Selection.hxx"
#include "db/Interface.hxx"
#include "db/Stats.hxx"
#include "Log.hxx"
#include "time/ChronoUtil.hxx"
#include "util/Math.hxx"

#ifdef _WIN32
#include "system/Clock.hxx"
#endif

#include <fmt/format.h>

#include <chrono>

#ifndef _WIN32
/**
 * The monotonic time stamp when MPD was started.  It is used to
 * calculate the uptime.
 */
static const std::chrono::steady_clock::time_point start_time =
	std::chrono::steady_clock::now();
#endif

#ifdef ENABLE_DATABASE

static DatabaseStats stats;

enum class StatsValidity : uint8_t {
	INVALID, VALID, FAILED,
};

static StatsValidity stats_validity = StatsValidity::INVALID;

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

	const DatabaseSelection selection("", true);

	try {
		stats = db.GetStats(selection);
		stats_validity = StatsValidity::VALID;
		return true;
	} catch (...) {
		LogError(std::current_exception());
		stats_validity = StatsValidity::FAILED;
		return false;
	}
}

static void
db_stats_print(Response &r, const Database &db)
{
	if (!stats_update(db))
		return;

	unsigned total_duration_s =
		std::chrono::duration_cast<std::chrono::seconds>(stats.total_duration).count();

	r.Fmt(FMT_STRING("artists: {}\n"
			 "albums: {}\n"
			 "songs: {}\n"
			 "db_playtime: {}\n"),
	      stats.artist_count,
	      stats.album_count,
	      stats.song_count,
	      total_duration_s);

	const auto update_stamp = db.GetUpdateStamp();
	if (!IsNegative(update_stamp))
		r.Fmt(FMT_STRING("db_update: {}\n"),
		      std::chrono::system_clock::to_time_t(update_stamp));
}

#endif

void
stats_print(Response &r, const Partition &partition)
{
#ifdef _WIN32
	const auto uptime = GetProcessUptimeS();
#else
	const auto uptime = std::chrono::steady_clock::now() - start_time;
#endif

	r.Fmt(FMT_STRING("uptime: {}\n"
			 "playtime: {}\n"),
	      std::chrono::duration_cast<std::chrono::seconds>(uptime).count(),
	      lround(partition.pc.GetTotalPlayTime().count()));

#ifdef ENABLE_DATABASE
	const Database *db = partition.instance.GetDatabase();
	if (db != nullptr)
		db_stats_print(r, *db);
#endif
}
