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
#include "Instance.hxx"
#include "Partition.hxx"
#include "Idle.hxx"
#include "Stats.hxx"
#include "db/DatabaseError.hxx"

#ifdef ENABLE_DATABASE

Database *
Instance::GetDatabase(Error &error)
{
	if (database == nullptr)
		error.Set(db_domain, DB_DISABLED, "No database");
	return database;
}

void
Instance::DeleteSong(const char *uri)
{
	partition->DeleteSong(uri);
}

#endif

void
Instance::TagModified()
{
	partition->TagModified();
}

void
Instance::SyncWithPlayer()
{
	partition->SyncWithPlayer();
}

#ifdef ENABLE_DATABASE

void
Instance::OnDatabaseModified()
{
	assert(database != nullptr);

	/* propagate the change to all subsystems */

	stats_invalidate();
	partition->DatabaseModified(*database);
	idle_add(IDLE_DATABASE);
}

#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS

void
Instance::FoundNeighbor(gcc_unused const NeighborInfo &info)
{
	idle_add(IDLE_NEIGHBOR);
}

void
Instance::LostNeighbor(gcc_unused const NeighborInfo &info)
{
	idle_add(IDLE_NEIGHBOR);
}

#endif
