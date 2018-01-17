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

#ifndef MPD_INSTANCE_HXX
#define MPD_INSTANCE_HXX

#include "check.h"
#include "event/Loop.hxx"
#include "event/Thread.hxx"
#include "event/MaskMonitor.hxx"
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

#include <list>

class ClientList;
struct Partition;
class StateFile;

/**
 * A utility class which, when used as the first base class, ensures
 * that the #EventLoop gets initialized before the other base classes.
 */
struct EventLoopHolder {
	EventLoop event_loop;
};

struct Instance final
	: EventLoopHolder
#if defined(ENABLE_DATABASE) || defined(ENABLE_NEIGHBOR_PLUGINS)
	,
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
	EventThread io_thread;

	MaskMonitor idle_monitor;

#ifdef ENABLE_NEIGHBOR_PLUGINS
	NeighborGlue *neighbors;
#endif

#ifdef ENABLE_DATABASE
	Database *database;

	/**
	 * This is really a #CompositeStorage.  To avoid heavy include
	 * dependencies, we declare it as just #Storage.
	 */
	Storage *storage = nullptr;

	UpdateService *update = nullptr;
#endif

	ClientList *client_list;

	std::list<Partition> partitions;

	StateFile *state_file = nullptr;

	Instance();

	/**
	 * Initiate shutdown.  Wrapper for EventLoop::Break().
	 */
	void Shutdown() {
		event_loop.Break();
	}

	void EmitIdle(unsigned mask) {
		idle_monitor.OrMask(mask);
	}

	/**
	 * Find a #Partition with the given name.  Returns nullptr if
	 * no such partition was found.
	 */
	gcc_pure
	Partition *FindPartition(const char *name) noexcept;

#ifdef ENABLE_DATABASE
	/**
	 * Returns the global #Database instance.  May return nullptr
	 * if this MPD configuration has no database (no
	 * music_directory was configured).
	 */
	Database *GetDatabase() {
		return database;
	}

	/**
	 * Returns the global #Database instance.  Throws
	 * DatabaseError if this MPD configuration has no database (no
	 * music_directory was configured).
	 */
	const Database &GetDatabaseOrThrow() const;
#endif

private:
#ifdef ENABLE_DATABASE
	void OnDatabaseModified() override;
	void OnDatabaseSongRemoved(const char *uri) override;
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
	/* virtual methods from class NeighborListener */
	void FoundNeighbor(const NeighborInfo &info) noexcept override;
	void LostNeighbor(const NeighborInfo &info) noexcept override;
#endif

	/* callback for #idle_monitor */
	void OnIdle(unsigned mask);
};

#endif
