/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "util/list.h"
#include "gcc.h"
#include "DatabaseVisitor.hxx"

#include <glib.h>
#include <stdbool.h>
#include <sys/types.h>

#define DEVICE_INARCHIVE (dev_t)(-1)
#define DEVICE_CONTAINER (dev_t)(-2)

#define directory_for_each_child(pos, directory) \
	list_for_each_entry(pos, &directory->children, siblings)

#define directory_for_each_child_safe(pos, n, directory) \
	list_for_each_entry_safe(pos, n, &directory->children, siblings)

#define directory_for_each_song(pos, directory) \
	list_for_each_entry(pos, &directory->songs, siblings)

#define directory_for_each_song_safe(pos, n, directory) \
	list_for_each_entry_safe(pos, n, &directory->songs, siblings)

#define directory_for_each_playlist(pos, directory) \
	list_for_each_entry(pos, &directory->playlists, siblings)

#define directory_for_each_playlist_safe(pos, n, directory) \
	list_for_each_entry_safe(pos, n, &directory->playlists, siblings)

struct song;
struct db_visitor;
class SongFilter;

struct directory {
	/**
	 * Pointers to the siblings of this directory within the
	 * parent directory.  It is unused (undefined) in the root
	 * directory.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	struct list_head siblings;

	/**
	 * A doubly linked list of child directories.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	struct list_head children;

	/**
	 * A doubly linked list of songs within this directory.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	struct list_head songs;

	struct list_head playlists;

	struct directory *parent;
	time_t mtime;
	ino_t inode;
	dev_t device;
	bool have_stat; /* not needed if ino_t == dev_t == 0 is impossible */
	char path[sizeof(long)];

	/**
	 * Generic constructor for #directory object.
	 */
	gcc_malloc
	static directory *NewGeneric(const char *path_utf8, directory *parent);

	/**
	 * Create a new root #directory object.
	 */
	gcc_malloc
	static directory *NewRoot() {
		return NewGeneric("", nullptr);
	}

	/**
	 * Free this #directory object (and the whole object tree within it),
	 * assuming it was already removed from the parent.
	 */
	void Free();

	/**
	 * Remove this #directory object from its parent and free it.  This
	 * must not be called with the root directory.
	 *
	 * Caller must lock the #db_mutex.
	 */
	void Delete();

	/**
	 * Create a new #directory object as a child of the given one.
	 *
	 * Caller must lock the #db_mutex.
	 *
	 * @param name_utf8 the UTF-8 encoded name of the new sub directory
	 */
	gcc_malloc
	directory *CreateChild(const char *name_utf8);

	/**
	 * Caller must lock the #db_mutex.
	 */
	gcc_pure
	const directory *FindChild(const char *name) const;

	gcc_pure
	directory *FindChild(const char *name) {
		const directory *cthis = this;
		return const_cast<directory *>(cthis->FindChild(name));
	}

	/**
	 * Look up a sub directory, and create the object if it does not
	 * exist.
	 *
	 * Caller must lock the #db_mutex.
	 */
	struct directory *MakeChild(const char *name_utf8) {
		struct directory *child = FindChild(name_utf8);
		if (child == nullptr)
			child = CreateChild(name_utf8);
		return child;
	}

	/**
	 * Looks up a directory by its relative URI.
	 *
	 * @param uri the relative URI
	 * @return the directory, or NULL if none was found
	 */
	gcc_pure
	directory *LookupDirectory(const char *uri);

	gcc_pure
	bool IsEmpty() const {
		return list_empty(&children) &&
			list_empty(&songs) &&
			list_empty(&playlists);
	}

	gcc_pure
	const char *GetPath() const {
		return path;
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
		return parent == NULL;
	}

	/**
	 * Look up a song in this directory by its name.
	 *
	 * Caller must lock the #db_mutex.
	 */
	gcc_pure
	const song *FindSong(const char *name_utf8) const;

	gcc_pure
	song *FindSong(const char *name_utf8) {
		const directory *cthis = this;
		return const_cast<song *>(cthis->FindSong(name_utf8));
	}

	/**
	 * Looks up a song by its relative URI.
	 *
	 * Caller must lock the #db_mutex.
	 *
	 * @param uri the relative URI
	 * @return the song, or NULL if none was found
	 */
	gcc_pure
	song *LookupSong(const char *uri);

	/**
	 * Add a song object to this directory.  Its "parent" attribute must
	 * be set already.
	 */
	void AddSong(song *song);

	/**
	 * Remove a song object from this directory (which effectively
	 * invalidates the song object, because the "parent" attribute becomes
	 * stale), but does not free it.
	 */
	void RemoveSong(song *song);

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
		  GError **error_r) const;
};

static inline bool
isRootDirectory(const char *name)
{
	return name[0] == 0 || (name[0] == '/' && name[1] == 0);
}

#endif
