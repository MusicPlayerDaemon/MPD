// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
