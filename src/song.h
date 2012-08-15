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

#ifndef MPD_SONG_H
#define MPD_SONG_H

#include "util/list.h"
#include "gcc.h"

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/time.h>

#define SONG_FILE	"file: "
#define SONG_TIME	"Time: "

struct song {
	/**
	 * Pointers to the siblings of this directory within the
	 * parent directory.  It is unused (undefined) if this song is
	 * not in the database.
	 *
	 * This attribute is protected with the global #db_mutex.
	 * Read access in the update thread does not need protection.
	 */
	struct list_head siblings;

	struct tag *tag;
	struct directory *parent;
	time_t mtime;

	/**
	 * Start of this sub-song within the file in milliseconds.
	 */
	unsigned start_ms;

	/**
	 * End of this sub-song within the file in milliseconds.
	 * Unused if zero.
	 */
	unsigned end_ms;

	char uri[sizeof(int)];
};

/**
 * A dummy #directory instance that is used for "detached" song
 * copies.
 */
extern struct directory detached_root;

G_BEGIN_DECLS

/** allocate a new song with a remote URL */
struct song *
song_remote_new(const char *uri);

/** allocate a new song with a local file name */
struct song *
song_file_new(const char *path, struct directory *parent);

/**
 * allocate a new song structure with a local file name and attempt to
 * load its metadata.  If all decoder plugin fail to read its meta
 * data, NULL is returned.
 */
struct song *
song_file_load(const char *path, struct directory *parent);

/**
 * Replaces the URI of a song object.  The given song object is
 * destroyed, and a newly allocated one is returned.  It does not
 * update the reference within the parent directory; the caller is
 * responsible for doing that.
 */
struct song *
song_replace_uri(struct song *song, const char *uri);

/**
 * Creates a "detached" song object.
 */
struct song *
song_detached_new(const char *uri);

/**
 * Creates a duplicate of the song object.  If the object is in the
 * database, it creates a "detached" copy of this song, see
 * song_is_detached().
 */
gcc_malloc
struct song *
song_dup_detached(const struct song *src);

void
song_free(struct song *song);

static inline bool
song_in_database(const struct song *song)
{
	return song->parent != NULL;
}

static inline bool
song_is_file(const struct song *song)
{
	return song_in_database(song) || song->uri[0] == '/';
}

static inline bool
song_is_detached(const struct song *song)
{
	assert(song != NULL);
	assert(song_in_database(song));

	return song->parent == &detached_root;
}

/**
 * Returns true if both objects refer to the same physical song.
 */
gcc_pure
bool
song_equals(const struct song *a, const struct song *b);

bool
song_file_update(struct song *song);

bool
song_file_update_inarchive(struct song *song);

/**
 * Returns the URI of the song in UTF-8 encoding, including its
 * location within the music directory.
 *
 * The return value is allocated on the heap, and must be freed by the
 * caller.
 */
char *
song_get_uri(const struct song *song);

double
song_get_duration(const struct song *song);

G_END_DECLS

#endif
