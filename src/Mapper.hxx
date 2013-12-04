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

/*
 * Maps directory and song objects to file system paths.
 */

#ifndef MPD_MAPPER_HXX
#define MPD_MAPPER_HXX

#include <string>

#include "Compiler.h"

#define PLAYLIST_FILE_SUFFIX ".m3u"

class AllocatedPath;
struct Directory;
struct Song;

void
mapper_init(AllocatedPath &&music_dir, AllocatedPath &&playlist_dir);

void mapper_finish(void);

/**
 * Return the absolute path of the music directory encoded in UTF-8 or
 * nullptr if no music directory was configured.
 */
gcc_const
const char *
mapper_get_music_directory_utf8(void);

/**
 * Return the absolute path of the music directory encoded in the
 * filesystem character set.
 */
gcc_const
const AllocatedPath &
mapper_get_music_directory_fs(void);

/**
 * Returns true if a music directory was configured.
 */
gcc_const
static inline bool
mapper_has_music_directory(void)
{
	return mapper_get_music_directory_utf8() != nullptr;
}

/**
 * If the specified absolute path points inside the music directory,
 * this function converts it to a relative path.  If not, it returns
 * the unmodified string pointer.
 */
gcc_pure
const char *
map_to_relative_path(const char *path_utf8);

/**
 * Determines the absolute file system path of a relative URI.  This
 * is basically done by converting the URI to the file system charset
 * and prepending the music directory.
 */
gcc_pure
AllocatedPath
map_uri_fs(const char *uri);

/**
 * Determines the file system path of a directory object.
 *
 * @param directory the directory object
 * @return the path in file system encoding, or nullptr if mapping failed
 */
gcc_pure
AllocatedPath
map_directory_fs(const Directory &directory);

/**
 * Determines the file system path of a directory's child (may be a
 * sub directory or a song).
 *
 * @param directory the parent directory object
 * @param name the child's name in UTF-8
 * @return the path in file system encoding, or nullptr if mapping failed
 */
gcc_pure
AllocatedPath
map_directory_child_fs(const Directory &directory, const char *name);

/**
 * Determines the file system path of a song.  This must not be a
 * remote song.
 *
 * @param song the song object
 * @return the path in file system encoding, or nullptr if mapping failed
 */
gcc_pure
AllocatedPath
map_song_fs(const Song &song);

/**
 * Maps a file system path (relative to the music directory or
 * absolute) to a relative path in UTF-8 encoding.
 *
 * @param path_fs a path in file system encoding
 * @return the relative path in UTF-8, or an empty string if mapping
 * failed
 */
gcc_pure
std::string
map_fs_to_utf8(const char *path_fs);

/**
 * Returns the playlist directory.
 */
gcc_const
const AllocatedPath &
map_spl_path(void);

/**
 * Maps a playlist name (without the ".m3u" suffix) to a file system
 * path.
 *
 * @return the path in file system encoding, or nullptr if mapping failed
 */
gcc_pure
AllocatedPath
map_spl_utf8_to_fs(const char *name);

#endif
