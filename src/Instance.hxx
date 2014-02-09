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

#ifndef MPD_INSTANCE_HXX
#define MPD_INSTANCE_HXX

#include "check.h"
#include "Compiler.h"

#ifdef ENABLE_NEIGHBOR_PLUGINS
#include "neighbor/Listener.hxx"
class NeighborGlue;
#endif

#ifdef ENABLE_DATABASE
#include "db/DatabaseListener.hxx"
class Database;
class Storage;
class UpdateService;
#endif

class EventLoop;
class Error;
class ClientList;
struct Partition;

struct Instance final
#if defined(ENABLE_DATABASE) || defined(ENABLE_NEIGHBOR_PLUGINS)
	:
#endif
#ifdef ENABLE_DATABASE
	public DatabaseListener
#ifdef ENABLE_NEIGHBOR_PLUGINS
	,
#endif
#endif
#ifdef ENABLE_NEIGHBOR_PLUGINS
	public NeighborListener
#endif
{
	EventLoop *event_loop;

#ifdef ENABLE_NEIGHBOR_PLUGINS
	NeighborGlue *neighbors;
#endif

#ifdef ENABLE_DATABASE
	Database *database;

	/**
	 * This is really a #CompositeStorage.  To avoid heavy include
	 * dependencies, we declare it as just #Storage.
	 */
	Storage *storage;

	UpdateService *update;
#endif

	ClientList *client_list;

	Partition *partition;

	Instance() {
#ifdef ENABLE_DATABASE
		storage = nullptr;
		update = nullptr;
#endif
	}

#ifdef ENABLE_DATABASE
	/**
	 * Returns the global #Database instance.  May return nullptr
	 * if this MPD configuration has no database (no
	 * music_directory was configured).
	 */
	Database *GetDatabase(Error &error);
#endif

	/**
	 * A tag in the play queue has been modified by the player
	 * thread.  Propagate the change to all subsystems.
	 */
	void TagModified();

	/**
	 * Synchronize the player with the play queue.
	 */
	void SyncWithPlayer();

private:
#ifdef ENABLE_DATABASE
	virtual void OnDatabaseModified() override;
	virtual void OnDatabaseSongRemoved(const LightSong &song) override;
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
	/* virtual methods from class NeighborListener */
	virtual void FoundNeighbor(const NeighborInfo &info) override;
	virtual void LostNeighbor(const NeighborInfo &info) override;
#endif
};

#endif
