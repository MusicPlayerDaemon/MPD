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

#ifndef MPD_FS_CHARSET_HXX
#define MPD_FS_CHARSET_HXX

#include "Traits.hxx"

/**
 * Gets file system character set name.
 */
[[gnu::const]]
const char *
GetFSCharset() noexcept;

/**
 * Throws std::runtime_error on error.
 */
void
SetFSCharset(const char *charset);

void
DeinitFSCharset() noexcept;

/**
 * Convert the path to UTF-8.
 *
 * Throws std::runtime_error on error.
 */
PathTraitsUTF8::string
PathToUTF8(PathTraitsFS::string_view path_fs);

/**
 * Convert the path from UTF-8.
 *
 * Throws std::runtime_error on error.
 */
PathTraitsFS::string
PathFromUTF8(PathTraitsUTF8::string_view path_utf8);

#endif
