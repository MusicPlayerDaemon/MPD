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

#ifndef MPD_UPDATE_IO_HXX
#define MPD_UPDATE_IO_HXX

#include <string_view>

struct Directory;
struct StorageFileInfo;
class Storage;
class StorageDirectoryReader;

/**
 * Wrapper for Storage::GetInfo() that logs errors instead of
 * returning them.
 */
bool
GetInfo(Storage &storage, const char *uri_utf8, StorageFileInfo &info) noexcept;

/**
 * Wrapper for LocalDirectoryReader::GetInfo() that logs errors
 * instead of returning them.
 */
bool
GetInfo(StorageDirectoryReader &reader, StorageFileInfo &info) noexcept;

[[gnu::pure]]
bool
DirectoryExists(Storage &storage, const Directory &directory) noexcept;

[[gnu::pure]]
bool
directory_child_is_regular(Storage &storage, const Directory &directory,
			   std::string_view name_utf8) noexcept;

/**
 * Checks if the given permissions on the mapped file are given.
 */
[[gnu::pure]]
bool
directory_child_access(const Storage &storage, const Directory &directory,
		       std::string_view name, int mode) noexcept;

#endif
