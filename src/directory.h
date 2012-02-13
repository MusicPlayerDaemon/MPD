/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_DIRECTORY_H
#define MPD_DIRECTORY_H

#include "check.h"
#include "util/list.h"

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
};

static inline bool
isRootDirectory(const char *name)
{
	return name[0] == 0 || (name[0] == '/' && name[1] == 0);
}

/**
 * Generic constructor for #directory object.
 */
G_GNUC_MALLOC
struct directory *
directory_new(const char *dirname, struct directory *parent);

/**
 * Create a new root #directory object.
 */
G_GNUC_MALLOC
static inline struct directory *
directory_new_root(void)
{
	return directory_new("", NULL);
}

/**
 * Free this #directory object (and the whole object tree within it),
 * assuming it was already removed from the parent.
 */
void
directory_free(struct directory *directory);

/**
 * Remove this #directory object from its parent and free it.  This
 * must not be called with the root directory.
 *
 * Caller must lock the #db_mutex.
 */
void
directory_delete(struct directory *directory);

static inline bool
directory_is_empty(const struct directory *directory)
{
	return list_empty(&directory->children) &&
		list_empty(&directory->songs) &&
		list_empty(&directory->playlists);
}

static inline const char *
directory_get_path(const struct directory *directory)
{
	return directory->path;
}

/**
 * Is this the root directory of the music database?
 */
static inline bool
directory_is_root(const struct directory *directory)
{
	return directory->parent == NULL;
}

/**
 * Returns the base name of the directory.
 */
G_GNUC_PURE
const char *
directory_get_name(const struct directory *directory);

/**
 * Caller must lock the #db_mutex.
 */
G_GNUC_PURE
struct directory *
directory_get_child(const struct directory *directory, const char *name);

/**
 * Create a new #directory object as a child of the given one.
 *
 * Caller must lock the #db_mutex.
 *
 * @param parent the parent directory the new one will be added to
 * @param name_utf8 the UTF-8 encoded name of the new sub directory
 */
G_GNUC_MALLOC
struct directory *
directory_new_child(struct directory *parent, const char *name_utf8);

/**
 * Look up a sub directory, and create the object if it does not
 * exist.
 *
 * Caller must lock the #db_mutex.
 */
static inline struct directory *
directory_make_child(struct directory *directory, const char *name_utf8)
{
	struct directory *child = directory_get_child(directory, name_utf8);
	if (child == NULL)
		child = directory_new_child(directory, name_utf8);
	return child;
}

/**
 * Caller must lock the #db_mutex.
 */
void
directory_prune_empty(struct directory *directory);

/**
 * Looks up a directory by its relative URI.
 *
 * @param directory the parent (or grandparent, ...) directory
 * @param uri the relative URI
 * @return the directory, or NULL if none was found
 */
struct directory *
directory_lookup_directory(struct directory *directory, const char *uri);

/**
 * Add a song object to this directory.  Its "parent" attribute must
 * be set already.
 */
void
directory_add_song(struct directory *directory, struct song *song);

/**
 * Remove a song object from this directory (which effectively
 * invalidates the song object, because the "parent" attribute becomes
 * stale), but does not free it.
 */
void
directory_remove_song(struct directory *directory, struct song *song);

/**
 * Look up a song in this directory by its name.
 *
 * Caller must lock the #db_mutex.
 */
G_GNUC_PURE
struct song *
directory_get_song(const struct directory *directory, const char *name_utf8);

/**
 * Looks up a song by its relative URI.
 *
 * Caller must lock the #db_mutex.
 *
 * @param directory the parent (or grandparent, ...) directory
 * @param uri the relative URI
 * @return the song, or NULL if none was found
 */
struct song *
directory_lookup_song(struct directory *directory, const char *uri);

/**
 * Sort all directory entries recursively.
 *
 * Caller must lock the #db_mutex.
 */
void
directory_sort(struct directory *directory);

/**
 * Caller must lock #db_mutex.
 */
bool
directory_walk(const struct directory *directory, bool recursive,
	       const struct db_visitor *visitor, void *ctx,
	       GError **error_r);

#endif
