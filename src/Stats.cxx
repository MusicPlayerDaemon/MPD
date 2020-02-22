/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "player/Control.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "db/Selection.hxx"
#include "db/Interface.hxx"
#include "db/Stats.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/SimpleDatabasePlugin.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "system/Clock.hxx"
#include "Log.hxx"
#ifdef ENABLE_DATABASE
#include "db/update/Service.hxx"
#define COMMAND_STATUS_UPDATING_DB	"updating_db"
#endif
#include "util/ChronoUtil.hxx"
#include "storage/CompositeStorage.hxx"

#include <chrono>

#include "util/Domain.hxx"
#include "Log.hxx"

static constexpr Domain stats_domain("stats");

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
static bool
getAllMounts(Storage *_composite, std::list<std::string> &list)
{
	if (_composite == nullptr) {
		return false;
	}

	CompositeStorage &composite = *(CompositeStorage *)_composite;

	list.clear();
	const auto visitor = [&list](const char *mount_uri,
				       gcc_unused const Storage &storage){
		if (*mount_uri != 0) {
			list.push_back(mount_uri);
		}
	};

	composite.VisitMounts(visitor);

	return true;
}

static void
db_stats_print(Response &r, const Database &db, const Partition &partition)
{
#ifdef ENABLE_DATABASE
	const UpdateService *update_service = partition.instance.update;
	unsigned updateJobId = update_service != nullptr
		? update_service->GetId()
		: 0;
	if (updateJobId != 0) {
		stats_validity = StatsValidity::INVALID;
		r.Format(COMMAND_STATUS_UPDATING_DB ": %i\n",
				  updateJobId);
	}
#endif
	if (!stats_update(db)) {
		FormatDefault(stats_domain, "%s %d get stats fail!", __func__, __LINE__);
 		return;
	}

	unsigned total_duration_s =
		std::chrono::duration_cast<std::chrono::seconds>(stats.total_duration).count();

	r.Format("artists: %u\n"
		 "albums: %u\n"
		 "songs: %u\n"
		 "db_playtime: %u\n",
		 stats.artist_count,
		 stats.album_count,
		 stats.song_count,
		 total_duration_s);

	std::chrono::system_clock::time_point update_stamp;
	std::list<std::string> list;
	Storage *_composite = partition.instance.storage;
	if (db.IsPlugin(simple_db_plugin) &&
		getAllMounts(_composite, list)) {
		SimpleDatabase *db2 = static_cast<SimpleDatabase*>(partition.instance.database);
		for (const auto &str : list) {
			db_lock();
			const auto lr = db2->GetRoot().LookupDirectory(str.c_str());
			db_unlock();
			if (lr.directory->IsMount()) {
				Database &_db2 = *(lr.directory->mounted_database);
				auto t = _db2.GetUpdateStamp();
				update_stamp = t;
			}
		}
	}

	if (!IsNegative(update_stamp))
		r.Format("db_update: %lu\n",
			 (unsigned long)std::chrono::system_clock::to_time_t(update_stamp));
}

#endif

void
stats_print(Response &r, const Partition &partition)
{
	r.Format("uptime: %u\n"
		 "playtime: %lu\n",
#ifdef _WIN32
		 GetProcessUptimeS(),
#else
		 (unsigned)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count(),
#endif
		 (unsigned long)(partition.pc.GetTotalPlayTime() + 0.5));

#ifdef ENABLE_DATABASE
	const Database *db = partition.instance.database;
	if (db != nullptr)
		db_stats_print(r, *db, partition);
#endif
}
