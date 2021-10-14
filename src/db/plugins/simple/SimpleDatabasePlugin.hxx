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

#ifndef MPD_SIMPLE_DATABASE_PLUGIN_HXX
#define MPD_SIMPLE_DATABASE_PLUGIN_HXX

#include "ExportedSong.hxx"
#include "db/Interface.hxx"
#include "db/Ptr.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/Manual.hxx"
#include "config.h"

#include <cassert>

struct ConfigBlock;
struct Directory;
struct DatabasePlugin;
class EventLoop;
class DatabaseListener;
class PrefixedLightSong;

class SimpleDatabase : public Database {
	AllocatedPath path;
	std::string path_utf8;

#ifdef ENABLE_ZLIB
	bool compress;
#endif

	bool hide_playlist_targets;

	/**
	 * The path where cache files for Mount() are located.
	 */
	AllocatedPath cache_path;

	Directory *root;

	std::chrono::system_clock::time_point mtime;

	/**
	 * A buffer for GetSong() when prefixing the #LightSong
	 * instance from a mounted #Database.
	 */
	mutable PrefixedLightSong *prefixed_light_song = nullptr;

	/**
	 * A buffer for GetSong().
	 */
	mutable Manual<ExportedSong> exported_song;

#ifndef NDEBUG
	mutable unsigned borrowed_song_count;
#endif

public:
	SimpleDatabase(const ConfigBlock &block);
	SimpleDatabase(AllocatedPath &&_path, bool _compress) noexcept;

	static DatabasePtr Create(EventLoop &main_event_loop,
				  EventLoop &io_event_loop,
				  DatabaseListener &listener,
				  const ConfigBlock &block);

	[[gnu::pure]]
	Directory &GetRoot() noexcept {
		assert(root != NULL);

		return *root;
	}

	void Save();

	/**
	 * Returns true if there is a valid database file on the disk.
	 */
	bool FileExists() const {
		return mtime >= std::chrono::system_clock::time_point(std::chrono::system_clock::duration::zero());
	}

	/**
	 * @param db the #Database to be mounted; must be "open"; on
	 * success, this object gains ownership of the given #Database
	 */
	[[gnu::nonnull]]
	void Mount(const char *uri, DatabasePtr db);

	/**
	 * Throws #std::runtime_error on error.
	 *
	 * @return false if the mounted database needs to be updated
	 */
	[[gnu::nonnull]]
	bool Mount(const char *local_uri, const char *storage_uri);

	[[gnu::nonnull]]
	bool Unmount(const char *uri) noexcept;

	/* virtual methods from class Database */
	void Open() override;
	void Close() noexcept override;

	const LightSong *GetSong(std::string_view uri_utf8) const override;
	void ReturnSong(const LightSong *song) const noexcept override;

	void Visit(const DatabaseSelection &selection,
		   VisitDirectory visit_directory,
		   VisitSong visit_song,
		   VisitPlaylist visit_playlist) const override;

	RecursiveMap<std::string> CollectUniqueTags(const DatabaseSelection &selection,
						    ConstBuffer<TagType> tag_types) const override;

	DatabaseStats GetStats(const DatabaseSelection &selection) const override;

	std::chrono::system_clock::time_point GetUpdateStamp() const noexcept override {
		return mtime;
	}

private:
	void Configure(const ConfigBlock &block);

	void Check() const;

	/**
	 * Throws #std::runtime_error on error.
	 */
	void Load();

	DatabasePtr LockUmountSteal(const char *uri) noexcept;
};

extern const DatabasePlugin simple_db_plugin;

#endif
