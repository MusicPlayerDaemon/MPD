// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
