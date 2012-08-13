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

/*
 * Maps directory and song objects to file system paths.
 */

#ifndef MPD_MAPPER_H
#define MPD_MAPPER_H

#include <glib.h>
#include <stdbool.h>

#define PLAYLIST_FILE_SUFFIX ".m3u"

struct directory;
struct song;

void mapper_init(const char *_music_dir, const char *_playlist_dir);

void mapper_finish(void);

/**
 * Return the absolute path of the music directory encoded in UTF-8.
 */
G_GNUC_CONST
const char *
mapper_get_music_directory_utf8(void);

/**
 * Return the absolute path of the music directory encoded in the
 * filesystem character set.
 */
G_GNUC_CONST
const char *
mapper_get_music_directory_fs(void);

/**
 * Returns true if a music directory was configured.
 */
G_GNUC_CONST
static inline bool
mapper_has_music_directory(void)
{
	return mapper_get_music_directory_utf8() != NULL;
}

/**
 * If the specified absolute path points inside the music directory,
 * this function converts it to a relative path.  If not, it returns
 * the unmodified string pointer.
 */
G_GNUC_PURE
const char *
map_to_relative_path(const char *path_utf8);

/**
 * Determines the absolute file system path of a relative URI.  This
 * is basically done by converting the URI to the file system charset
 * and prepending the music directory.
 */
G_GNUC_MALLOC
char *
map_uri_fs(const char *uri);

/**
 * Determines the file system path of a directory object.
 *
 * @param directory the directory object
 * @return the path in file system encoding, or NULL if mapping failed
 */
G_GNUC_MALLOC
char *
map_directory_fs(const struct directory *directory);

/**
 * Determines the file system path of a directory's child (may be a
 * sub directory or a song).
 *
 * @param directory the parent directory object
 * @param name the child's name in UTF-8
 * @return the path in file system encoding, or NULL if mapping failed
 */
G_GNUC_MALLOC
char *
map_directory_child_fs(const struct directory *directory, const char *name);

/**
 * Determines the file system path of a song.  This must not be a
 * remote song.
 *
 * @param song the song object
 * @return the path in file system encoding, or NULL if mapping failed
 */
G_GNUC_MALLOC
char *
map_song_fs(const struct song *song);

/**
 * Maps a file system path (relative to the music directory or
 * absolute) to a relative path in UTF-8 encoding.
 *
 * @param path_fs a path in file system encoding
 * @return the relative path in UTF-8, or NULL if mapping failed
 */
G_GNUC_MALLOC
char *
map_fs_to_utf8(const char *path_fs);

/**
 * Returns the playlist directory.
 */
G_GNUC_CONST
const char *
map_spl_path(void);

/**
 * Maps a playlist name (without the ".m3u" suffix) to a file system
 * path.  The return value is allocated on the heap and must be freed
 * with g_free().
 *
 * @return the path in file system encoding, or NULL if mapping failed
 */
G_GNUC_PURE
char *
map_spl_utf8_to_fs(const char *name);

#endif
