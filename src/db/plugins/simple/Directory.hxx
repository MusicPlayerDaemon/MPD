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

#ifndef MPD_DIRECTORY_HXX
#define MPD_DIRECTORY_HXX

#include "check.h"
#include "Compiler.h"
#include "db/Visitor.hxx"
#include "db/PlaylistVector.hxx"
#include "Song.hxx"

#include <boost/intrusive/list.hpp>

#include <string>

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

struct db_visitor;
class SongFilter;
class Error;
class Database;

struct Directory {
	static constexpr auto link_mode = boost::intrusive::normal_link;
	typedef boost::intrusive::link_mode<link_mode> LinkMode;
	typedef boost::intrusive::list_member_hook<LinkMode> Hook;

	struct Disposer {
		void operator()(Directory *directory) const {
			delete directory;
		}
	};

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

	Directory *parent;
	time_t mtime;
	unsigned inode, device;

	std::string path;

	/**
	 * If this is not nullptr, then this directory does not really
	 * exist, but is a mount point for another #Database.
	 */
	Database *mounted_database;

public:
	Directory(std::string &&_path_utf8, Directory *_parent);
	~Directory();

	/**
	 * Create a new root #Directory object.
	 */
	gcc_malloc
	static Directory *NewRoot() {
		return new Directory(std::string(), nullptr);
	}

	bool IsMount() const {
		return mounted_database != nullptr;
	}

	/**
	 * Remove this #Directory object from its parent and free it.  This
	 * must not be called with the root Directory.
	 *
	 * Caller must lock the #db_mutex.
	 */
	void Delete();

	/**
	 * Create a new #Directory object as a child of the given one.
	 *
	 * Caller must lock the #db_mutex.
	 *
	 * @param name_utf8 the UTF-8 encoded name of the new sub directory
	 */
	gcc_malloc
	Directory *CreateChild(const char *name_utf8);

	/**
	 * Caller must lock the #db_mutex.
	 */
	gcc_pure
	const Directory *FindChild(const char *name) const;

	gcc_pure
	Directory *FindChild(const char *name) {
		const Directory *cthis = this;
		return const_cast<Directory *>(cthis->FindChild(name));
	}

	/**
	 * Look up a sub directory, and create the object if it does not
	 * exist.
	 *
	 * Caller must lock the #db_mutex.
	 */
	Directory *MakeChild(const char *name_utf8) {
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
		 * The remaining URI part (without leading slash) or
		 * nullptr if the given URI was consumed completely.
		 */
		const char *uri;
	};

	/**
	 * Looks up a directory by its relative URI.
	 *
	 * @param uri the relative URI
	 * @return the Directory, or nullptr if none was found
	 */
	gcc_pure
	LookupResult LookupDirectory(const char *uri);

	gcc_pure
	bool IsEmpty() const {
		return children.empty() &&
			songs.empty() &&
			playlists.empty();
	}

	gcc_pure
	const char *GetPath() const {
		return path.c_str();
	}

	/**
	 * Returns the base name of the directory.
	 */
	gcc_pure
	const char *GetName() const;

	/**
	 * Is this the root directory of the music database?
	 */
	gcc_pure
	bool IsRoot() const {
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
	gcc_pure
	const Song *FindSong(const char *name_utf8) const;

	gcc_pure
	Song *FindSong(const char *name_utf8) {
		const Directory *cthis = this;
		return const_cast<Song *>(cthis->FindSong(name_utf8));
	}

	/**
	 * Add a song object to this directory.  Its "parent" attribute must
	 * be set already.
	 */
	void AddSong(Song *song);

	/**
	 * Remove a song object from this directory (which effectively
	 * invalidates the song object, because the "parent" attribute becomes
	 * stale), but does not free it.
	 */
	void RemoveSong(Song *song);

	/**
	 * Caller must lock the #db_mutex.
	 */
	void PruneEmpty();

	/**
	 * Sort all directory entries recursively.
	 *
	 * Caller must lock the #db_mutex.
	 */
	void Sort();

	/**
	 * Caller must lock #db_mutex.
	 */
	bool Walk(bool recursive, const SongFilter *match,
		  VisitDirectory visit_directory, VisitSong visit_song,
		  VisitPlaylist visit_playlist,
		  Error &error) const;

	gcc_pure
	LightDirectory Export() const;
};

#endif
