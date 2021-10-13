/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "config.h"

#include <string>

#define PLAYLIST_FILE_SUFFIX ".m3u"

class Path;
class AllocatedPath;

void
mapper_init(AllocatedPath &&playlist_dir);

#ifdef ENABLE_DATABASE

/**
 * Determines the absolute file system path of a relative URI.  This
 * is basically done by converting the URI to the file system charset
 * and prepending the music directory.
 */
[[gnu::pure]]
AllocatedPath
map_uri_fs(const char *uri) noexcept;

/**
 * Maps a file system path (relative to the music directory or
 * absolute) to a relative path in UTF-8 encoding.
 *
 * @param path_fs a path in file system encoding
 * @return the relative path in UTF-8, or an empty string if mapping
 * failed
 */
[[gnu::pure]]
std::string
map_fs_to_utf8(Path path_fs) noexcept;

#endif

/**
 * Returns the playlist directory.
 */
[[gnu::const]]
const AllocatedPath &
map_spl_path() noexcept;

/**
 * Maps a playlist name (without the ".m3u" suffix) to a file system
 * path.
 *
 * @return the path in file system encoding, or nullptr if mapping failed
 */
[[gnu::pure]]
AllocatedPath
map_spl_utf8_to_fs(const char *name) noexcept;

#endif
