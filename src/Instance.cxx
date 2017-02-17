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
#include "Instance.hxx"
#include "Partition.hxx"
#include "Idle.hxx"
#include "Stats.hxx"

#ifdef ENABLE_DATABASE
#include "db/DatabaseError.hxx"

#ifdef ENABLE_SQLITE
#include "sticker/StickerDatabase.hxx"
#include "sticker/SongSticker.hxx"
#endif
#endif

#include <stdexcept>

Instance::Instance()
	:idle_monitor(event_loop, BIND_THIS_METHOD(OnIdle))
{
}

#ifdef ENABLE_DATABASE

const Database &
Instance::GetDatabaseOrThrow() const
{
	if (database == nullptr)
		throw DatabaseError(DatabaseErrorCode::DISABLED,
				    "No database");

	return *database;
}

void
Instance::OnDatabaseModified()
{
	assert(database != nullptr);

	/* propagate the change to all subsystems */

	stats_invalidate();
	partition->DatabaseModified(*database);
}

void
Instance::OnDatabaseSongRemoved(const char *uri)
{
	assert(database != nullptr);

#ifdef ENABLE_SQLITE
	/* if the song has a sticker, remove it */
	if (sticker_enabled()) {
		try {
			sticker_song_delete(uri);
		} catch (const std::runtime_error &) {
		}
	}
#endif

	partition->StaleSong(uri);
}

#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS

void
Instance::FoundNeighbor(gcc_unused const NeighborInfo &info)
{
	partition->EmitIdle(IDLE_NEIGHBOR);
}

void
Instance::LostNeighbor(gcc_unused const NeighborInfo &info)
{
	partition->EmitIdle(IDLE_NEIGHBOR);
}

#endif
