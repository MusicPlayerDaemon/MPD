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

#ifndef MPD_SONG_H
#define MPD_SONG_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/time.h>

#define SONG_FILE	"file: "
#define SONG_TIME	"Time: "

struct song {
	struct tag *tag;
	struct directory *parent;
	time_t mtime;
	char uri[sizeof(int)];
};

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

void
song_free(struct song *song);

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

#endif
