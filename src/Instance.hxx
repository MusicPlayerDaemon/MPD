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

#ifndef MPD_INSTANCE_HXX
#define MPD_INSTANCE_HXX

#include "config.h"
#include "event/Loop.hxx"
#include "event/Thread.hxx"
#include "event/MaskMonitor.hxx"

#ifdef ENABLE_SYSTEMD_DAEMON
#include "lib/systemd/Watchdog.hxx"
#endif

#ifdef ENABLE_CURL
#include "RemoteTagCacheHandler.hxx"
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
#include "neighbor/Listener.hxx"
class NeighborGlue;
#endif

#ifdef ENABLE_DATABASE
#include "db/DatabaseListener.hxx"
#include "db/Ptr.hxx"
class Storage;
class UpdateService;
#ifdef ENABLE_INOTIFY
class InotifyUpdate;
#endif
#endif

#include <memory>
#include <list>

class ClientList;
struct Partition;
class StateFile;
class RemoteTagCache;
class StickerDatabase;
class InputCacheManager;

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
#ifdef ENABLE_CURL
	, public RemoteTagCacheHandler
#endif
{
	/**
	 * A thread running an #EventLoop for non-blocking (bulk) I/O.
	 */
	EventThread io_thread;

	/**
	 * Another thread running an #EventLoop for non-blocking
	 * (real-time) I/O.  This is used instead of #io_thread for
	 * events which require low latency, e.g. for filling hardware
	 * ring buffers.
	 */
	EventThread rtio_thread;

#ifdef ENABLE_SYSTEMD_DAEMON
	Systemd::Watchdog systemd_watchdog;
#endif

	std::unique_ptr<InputCacheManager> input_cache;

	/**
	 * Monitor for global idle events to be broadcasted to all
	 * partitions.
	 */
	MaskMonitor idle_monitor;

#ifdef ENABLE_NEIGHBOR_PLUGINS
	std::unique_ptr<NeighborGlue> neighbors;
#endif

#ifdef ENABLE_DATABASE
	DatabasePtr database;

	/**
	 * This is really a #CompositeStorage.  To avoid heavy include
	 * dependencies, we declare it as just #Storage.
	 */
	Storage *storage = nullptr;

	UpdateService *update = nullptr;

#ifdef ENABLE_INOTIFY
	std::unique_ptr<InotifyUpdate> inotify_update;
#endif
#endif

#ifdef ENABLE_CURL
	std::unique_ptr<RemoteTagCache> remote_tag_cache;
#endif

	std::unique_ptr<ClientList> client_list;

	std::list<Partition> partitions;

	std::unique_ptr<StateFile> state_file;

#ifdef ENABLE_SQLITE
	std::unique_ptr<StickerDatabase> sticker_database;
#endif

	Instance();
	~Instance() noexcept;

	/**
	 * Wrapper for EventLoop::Break().  Call to initiate shutdown.
	 */
	void Break() noexcept {
		event_loop.Break();
	}

	/**
	 * Emit an "idle" event to all clients of all partitions.
	 *
	 * This method can be called from any thread.
	 */
	void EmitIdle(unsigned mask) noexcept {
		idle_monitor.OrMask(mask);
	}

	/**
	 * Notify the #Instance that the state has been modified, and
	 * the #StateFile may need to be saved.
	 *
	 * This method must be called from the main thread.
	 */
	void OnStateModified() noexcept;

	/**
	 * Find a #Partition with the given name.  Returns nullptr if
	 * no such partition was found.
	 */
	[[gnu::pure]]
	Partition *FindPartition(const char *name) noexcept;

	void DeletePartition(Partition &partition) noexcept;

	void BeginShutdownPartitions() noexcept;

#ifdef ENABLE_DATABASE
	/**
	 * Returns the global #Database instance.  May return nullptr
	 * if this MPD configuration has no database (no
	 * music_directory was configured).
	 */
	Database *GetDatabase() noexcept {
		return database.get();
	}

	/**
	 * Returns the global #Database instance.  Throws
	 * DatabaseError if this MPD configuration has no database (no
	 * music_directory was configured).
	 */
	const Database &GetDatabaseOrThrow() const;
#endif

#ifdef ENABLE_SQLITE
	bool HasStickerDatabase() const noexcept {
		return sticker_database != nullptr;
	}
#endif

	void BeginShutdownUpdate() noexcept;

#ifdef ENABLE_CURL
	void LookupRemoteTag(const char *uri) noexcept;
#else
	void LookupRemoteTag(const char *) noexcept {
		/* no-op */
	}
#endif

	void FlushCaches() noexcept;

private:
#ifdef ENABLE_DATABASE
	/* virtual methods from class DatabaseListener */
	void OnDatabaseModified() noexcept override;
	void OnDatabaseSongRemoved(const char *uri) noexcept override;
#endif

#ifdef ENABLE_NEIGHBOR_PLUGINS
	/* virtual methods from class NeighborListener */
	void FoundNeighbor(const NeighborInfo &info) noexcept override;
	void LostNeighbor(const NeighborInfo &info) noexcept override;
#endif

#ifdef ENABLE_CURL
	/* virtual methods from class RemoteTagCacheHandler */
	void OnRemoteTag(const char *uri, const Tag &tag) noexcept override;
#endif

	/* callback for #idle_monitor */
	void OnIdle(unsigned mask) noexcept;
};

#endif
