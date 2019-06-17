/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_PLAYLIST_FILE_HXX
#define MPD_PLAYLIST_FILE_HXX

#include <vector>
#include <string>

struct ConfigData;
class DetachedSong;
class SongLoader;
class PlaylistVector;
class AllocatedPath;

typedef std::vector<std::string> PlaylistFileContents;

extern bool playlist_saveAbsolutePaths;

/**
 * Perform some global initialization, e.g. load configuration values.
 */
void
spl_global_init(const ConfigData &config);

/**
 * Determines whether the specified string is a valid name for a
 * stored playlist.
 */
bool
spl_valid_name(const char *name_utf8);

AllocatedPath
spl_map_to_fs(const char *name_utf8);

/**
 * Returns a list of stored_playlist_info struct pointers.
 */
PlaylistVector
ListPlaylistFiles();

PlaylistFileContents
LoadPlaylistFile(const char *utf8path);

void
spl_move_index(const char *utf8path, unsigned src, unsigned dest);

void
spl_clear(const char *utf8path);

void
spl_delete(const char *name_utf8);

void
spl_remove_index(const char *utf8path, unsigned pos);

void
spl_append_song(const char *utf8path, const DetachedSong &song);

/**
 * Throws #std::runtime_error on error.
 */
void
spl_append_uri(const char *path_utf8,
	       const SongLoader &loader, const char *uri_utf8);

void
spl_rename(const char *utf8from, const char *utf8to);

#endif
