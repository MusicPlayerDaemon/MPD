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

#ifndef MPD_DIRECTORY_HXX
#define MPD_DIRECTORY_HXX

#include "Ptr.hxx"
#include "db/Visitor.hxx"
#include "db/PlaylistVector.hxx"
#include "db/Ptr.hxx"
#include "Song.hxx"

#include <boost/intrusive/list.hpp>

#include <string>
#include <string_view>

/**
 * Virtual directory that is really an archive file or a folder inside
 * the archive (special value for Directory::device).
 */
static constexpr unsigned DEVICE_INARCHIVE = -1;

/**
 * Virtual directory that is really a song file with one or more "sub"
 * songs as specified by DecoderPlugin::container_scan() (special
 * value for Directory::device).
 */
static constexpr unsigned DEVICE_CONTAINER = -2;

/**
 * Virtual directory that is really a playlist file (special value for
 * Directory::device).
 */
static constexpr unsigned DEVICE_PLAYLIST = -3;

class SongFilter;

struct Directory {
	static constexpr auto link_mode = boost::intrusive::normal_link;
	typedef boost::intrusive::link_mode<link_mode> LinkMode;
	typedef boost::intrusive::list_member_hook<LinkMode> Hook;

	/**
	 * Pointers to the siblings of this directory within the
	 * parent directory.  It is unused (undefined) in the root
	 * directory.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	Hook siblings;

	typedef boost::intrusive::member_hook<Directory, Hook,
					      &Directory::siblings> SiblingsHook;
	typedef boost::intrusive::list<Directory, SiblingsHook,
				       boost::intrusive::constant_time_size<false>> List;

	/**
	 * A doubly linked list of child directories.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	List children;

	/**
	 * A doubly linked list of songs within this directory.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	SongList songs;

	PlaylistVector playlists;

	Directory *const parent;

	std::chrono::system_clock::time_point mtime =
		std::chrono::system_clock::time_point::min();

	uint64_t inode = 0, device = 0;

	const std::string path;

	/**
	 * If this is not nullptr, then this directory does not really
	 * exist, but is a mount point for another #Database.
	 */
	DatabasePtr mounted_database;

public:
	Directory(std::string &&_path_utf8, Directory *_parent) noexcept;
	~Directory() noexcept;

	/**
	 * Create a new root #Directory object.
	 */
	[[gnu::malloc]] [[gnu::returns_nonnull]]
	static Directory *NewRoot() noexcept {
		return new Directory(std::string(), nullptr);
	}

	bool IsPlaylist() const noexcept {
		return device == DEVICE_PLAYLIST;
	}

	/**
	 * Is this really a regular file which is being treated like a
	 * directory?
	 */
	bool IsReallyAFile() const noexcept {
		return device == DEVICE_INARCHIVE ||
			IsPlaylist() ||
			device == DEVICE_CONTAINER;
	}

	bool IsMount() const noexcept {
		return mounted_database != nullptr;
	}

	/**
	 * Checks whether this is a "special" directory
	 * (e.g. #DEVICE_PLAYLIST) and whether the underlying plugin
	 * is available.
	 */
	[[gnu::pure]]
	bool IsPluginAvailable() const noexcept;

	/**
	 * Remove this #Directory object from its parent and free it.  This
	 * must not be called with the root Directory.
	 *
	 * Caller must lock the #db_mutex.
	 */
	void Delete() noexcept;

	/**
	 * Create a new #Directory object as a child of the given one.
	 *
	 * Caller must lock the #db_mutex.
	 *
	 * @param name_utf8 the UTF-8 encoded name of the new sub directory
	 */
	Directory *CreateChild(std::string_view name_utf8) noexcept;

	/**
	 * Caller must lock the #db_mutex.
	 */
	[[gnu::pure]]
	const Directory *FindChild(std::string_view name) const noexcept;

	[[gnu::pure]]
	Directory *FindChild(std::string_view name) noexcept {
		const Directory *cthis = this;
		return const_cast<Directory *>(cthis->FindChild(name));
	}

	/**
	 * Look up a sub directory, and create the object if it does not
	 * exist.
	 *
	 * Caller must lock the #db_mutex.
	 */
	Directory *MakeChild(std::string_view name_utf8) noexcept {
		Directory *child = FindChild(name_utf8);
		if (child == nullptr)
			child = CreateChild(name_utf8);
		return child;
	}

	struct LookupResult {
		/**
		 * The last directory that was found.  If the given
		 * URI could not be resolved at all, then this is the
		 * root directory.
		 */
		Directory *directory;

		/**
		 * The URI part which resolved to the #directory.
		 */
		std::string_view uri;

		/**
		 * The remaining URI part (without leading slash) or
		 * empty if the given URI was consumed completely.
		 */
		std::string_view rest;
	};

	/**
	 * Looks up a directory by its relative URI.
	 *
	 * @param uri the relative URI
	 */
	[[gnu::pure]]
	LookupResult LookupDirectory(std::string_view uri) noexcept;

	[[gnu::pure]]
	Song *LookupTargetSong(std::string_view target) noexcept;

	[[gnu::pure]]
	bool IsEmpty() const noexcept {
		return children.empty() &&
			songs.empty() &&
			playlists.empty();
	}

	[[gnu::pure]]
	const char *GetPath() const noexcept {
		return path.c_str();
	}

	/**
	 * Returns the base name of the directory.
	 */
	[[gnu::pure]]
	const char *GetName() const noexcept;

	/**
	 * Is this the root directory of the music database?
	 */
	[[gnu::pure]]
	bool IsRoot() const noexcept {
		return parent == nullptr;
	}

	template<typename T>
	void ForEachChildSafe(T &&t) {
		const auto end = children.end();
		for (auto i = children.begin(), next = i; i != end; i = next) {
			next = std::next(i);
			t(*i);
		}
	}

	template<typename T>
	void ForEachSongSafe(T &&t) {
		const auto end = songs.end();
		for (auto i = songs.begin(), next = i; i != end; i = next) {
			next = std::next(i);
			t(*i);
		}
	}

	/**
	 * Look up a song in this directory by its name.
	 *
	 * Caller must lock the #db_mutex.
	 */
	[[gnu::pure]]
	const Song *FindSong(std::string_view name_utf8) const noexcept;

	[[gnu::pure]]
	Song *FindSong(std::string_view name_utf8) noexcept {
		const Directory *cthis = this;
		return const_cast<Song *>(cthis->FindSong(name_utf8));
	}

	/**
	 * Add a song object to this directory.  Its "parent" attribute must
	 * be set already.
	 */
	void AddSong(SongPtr song) noexcept;

	/**
	 * Remove a song object from this directory (which effectively
	 * invalidates the song object, because the "parent" attribute becomes
	 * stale), and return ownership to the caller.
	 */
	SongPtr RemoveSong(Song *song) noexcept;

	/**
	 * Caller must lock the #db_mutex.
	 */
	void PruneEmpty() noexcept;

	/**
	 * Sort all directory entries recursively.
	 *
	 * Caller must lock the #db_mutex.
	 */
	void Sort() noexcept;

	/**
	 * Caller must lock #db_mutex.
	 */
	void Walk(bool recursive, const SongFilter *match,
		  bool hide_playlist_targets,
		  const VisitDirectory& visit_directory, const VisitSong& visit_song,
		  const VisitPlaylist& visit_playlist) const;

	[[gnu::pure]]
	LightDirectory Export() const noexcept;
};

#endif
