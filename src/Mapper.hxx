// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
