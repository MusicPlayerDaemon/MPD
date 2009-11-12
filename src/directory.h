/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "dirvec.h"
#include "songvec.h"

#include <stdbool.h>
#include <sys/types.h>

#define DIRECTORY_DIR		"directory: "

#define DEVICE_INARCHIVE	(unsigned)(-1)
#define DEVICE_CONTAINER	(unsigned)(-2)

struct directory {
	struct dirvec children;
	struct songvec songs;
	struct directory *parent;
	time_t mtime;
	ino_t inode;
	dev_t device;
	unsigned stat; /* not needed if ino_t == dev_t == 0 is impossible */
	char path[sizeof(long)];
};

static inline bool
isRootDirectory(const char *name)
{
	return name[0] == 0 || (name[0] == '/' && name[1] == 0);
}

struct directory *
directory_new(const char *dirname, struct directory *parent);

void
directory_free(struct directory *directory);

static inline bool
directory_is_empty(const struct directory *directory)
{
	return directory->children.nr == 0 && directory->songs.nr == 0;
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
const char *
directory_get_name(const struct directory *directory);

static inline struct directory *
directory_get_child(const struct directory *directory, const char *name)
{
	return dirvec_find(&directory->children, name);
}

static inline struct directory *
directory_new_child(struct directory *directory, const char *name)
{
	struct directory *subdir = directory_new(name, directory);
	dirvec_add(&directory->children, subdir);
	return subdir;
}

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
 * Looks up a song by its relative URI.
 *
 * @param directory the parent (or grandparent, ...) directory
 * @param uri the relative URI
 * @return the song, or NULL if none was found
 */
struct song *
directory_lookup_song(struct directory *directory, const char *uri);

void
directory_sort(struct directory *directory);

int
directory_walk(struct directory *directory,
	       int (*forEachSong)(struct song *, void *),
	       int (*forEachDir)(struct directory *, void *),
	       void *data);

#endif
