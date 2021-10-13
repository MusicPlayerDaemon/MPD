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

#include "config.h"
#include "Instance.hxx"
#include "Partition.hxx"
#include "IdleFlags.hxx"
#include "StateFile.hxx"
#include "Stats.hxx"
#include "client/List.hxx"
#include "input/cache/Manager.hxx"

#ifdef ENABLE_CURL
#include "RemoteTagCache.hxx"
#include "util/UriExtract.hxx"
#endif

#ifdef ENABLE_DATABASE
#include "db/DatabaseError.hxx"
#include "db/Interface.hxx"
#include "db/update/Service.hxx"
#include "storage/StorageInterface.hxx"

#ifdef ENABLE_INOTIFY
#include "db/update/InotifyUpdate.hxx"
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
#include "neighbor/Glue.hxx"
#endif

#ifdef ENABLE_SQLITE
#include "sticker/Database.hxx"
#include "sticker/SongSticker.hxx"
#endif
#endif

Instance::Instance()
	:rtio_thread(true),
#ifdef ENABLE_SYSTEMD_DAEMON
	 systemd_watchdog(event_loop),
#endif
	 idle_monitor(event_loop, BIND_THIS_METHOD(OnIdle))
{
}

Instance::~Instance() noexcept
{
#ifdef ENABLE_DATABASE
	delete update;

	if (database != nullptr) {
		database->Close();
		database.reset();
	}

	delete storage;
#endif
}

void
Instance::OnStateModified() noexcept
{
	if (state_file)
		state_file->CheckModified();
}

Partition *
Instance::FindPartition(const char *name) noexcept
{
	for (auto &partition : partitions)
		if (partition.name == name)
			return &partition;

	return nullptr;
}

void
Instance::DeletePartition(Partition &partition) noexcept
{
	// TODO: use boost::intrusive::list to avoid this loop
	for (auto i = partitions.begin();; ++i) {
		assert(i != partitions.end());

		if (&*i == &partition) {
			partitions.erase(i);
			break;
		}
	}
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
Instance::OnDatabaseModified() noexcept
{
	assert(database != nullptr);

	/* propagate the change to all subsystems */

	stats_invalidate();

	for (auto &partition : partitions)
		partition.DatabaseModified(*database);
}

void
Instance::OnDatabaseSongRemoved(const char *uri) noexcept
{
	assert(database != nullptr);

#ifdef ENABLE_SQLITE
	/* if the song has a sticker, remove it */
	if (HasStickerDatabase()) {
		try {
			sticker_song_delete(*sticker_database, uri);
		} catch (...) {
		}
	}
#endif

	for (auto &partition : partitions)
		partition.StaleSong(uri);
}

#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS

void
Instance::FoundNeighbor([[maybe_unused]] const NeighborInfo &info) noexcept
{
	EmitIdle(IDLE_NEIGHBOR);
}

void
Instance::LostNeighbor([[maybe_unused]] const NeighborInfo &info) noexcept
{
	EmitIdle(IDLE_NEIGHBOR);
}

#endif

#ifdef ENABLE_CURL

void
Instance::LookupRemoteTag(const char *uri) noexcept
{
	if (!uri_has_scheme(uri))
		return;

	if (!remote_tag_cache)
		remote_tag_cache = std::make_unique<RemoteTagCache>(event_loop,
								    *this);

	remote_tag_cache->Lookup(uri);
}

void
Instance::OnRemoteTag(const char *uri, const Tag &tag) noexcept
{
	if (!tag.IsDefined())
		/* boring */
		return;

	for (auto &partition : partitions)
		partition.TagModified(uri, tag);
}

#endif

void
Instance::OnIdle(unsigned flags) noexcept
{
	/* broadcast to all partitions */
	for (auto &partition : partitions)
		partition.EmitIdle(flags);
}

void
Instance::FlushCaches() noexcept
{
	if (input_cache)
		input_cache->Flush();
}
