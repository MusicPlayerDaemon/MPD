/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Maps directory and song objects to file system paths.
 */

#ifndef MPD_MAPPER_H
#define MPD_MAPPER_H

#define PLAYLIST_FILE_SUFFIX "m3u"

struct directory;
struct song;

void mapper_init(void);

void mapper_finish(void);

/**
 * Determines the absolute file system path of a relative URI.  This
 * is basically done by converting the URI to the file system charset
 * and prepending the music directory.
 */
char *
map_uri_fs(const char *uri);

/**
 * Determines the file system path of a directory object.
 *
 * @param directory the directory object
 * @param a buffer which is MPD_PATH_MAX bytes long
 * @return the path in file system encoding, or NULL if mapping failed
 */
char *
map_directory_fs(const struct directory *directory);

/**
 * Determines the file system path of a directory's child (may be a
 * sub directory or a song).
 *
 * @param directory the parent directory object
 * @param name the child's name in UTF-8
 * @param a buffer which is MPD_PATH_MAX bytes long
 * @return the path in file system encoding, or NULL if mapping failed
 */
char *
map_directory_child_fs(const struct directory *directory, const char *name);

/**
 * Determines the file system path of a song.  This must not be a
 * remote song.
 *
 * @param song the song object
 * @param a buffer which is MPD_PATH_MAX bytes long
 * @return the path in file system encoding, or NULL if mapping failed
 */
char *
map_song_fs(const struct song *song);

/**
 * Maps a file system path (relative to the music directory or
 * absolute) to a relative path in UTF-8 encoding.
 *
 * @param path_fs a path in file system encoding
 * @param buffer a buffer which is MPD_PATH_MAX bytes long
 * @return the relative path in UTF-8, or NULL if mapping failed
 */
const char *
map_fs_to_utf8(const char *path_fs, char *buffer);

/**
 * Returns the playlist directory.
 */
const char *
map_spl_path(void);

/**
 * Maps a playlist name (without the ".m3u" suffix) to a file system
 * path.  The return value is allocated on the heap and must be freed
 * with g_free().
 */
char *
map_spl_utf8_to_fs(const char *name);

#endif
