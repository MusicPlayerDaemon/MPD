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

#include "UpdateIO.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "storage/FileInfo.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/AllocatedPath.hxx"
#include "Log.hxx"

#include <cerrno>

bool
GetInfo(Storage &storage, const char *uri_utf8, StorageFileInfo &info) noexcept
try {
	info = storage.GetInfo(uri_utf8, true);
	return true;
} catch (...) {
	LogError(std::current_exception());
	return false;
}

bool
GetInfo(StorageDirectoryReader &reader, StorageFileInfo &info) noexcept
try {
	info = reader.GetInfo(true);
	return true;
} catch (...) {
	LogError(std::current_exception());
	return false;
}

bool
DirectoryExists(Storage &storage, const Directory &directory) noexcept
{
	StorageFileInfo info;

	try {
		info = storage.GetInfo(directory.GetPath(), true);
	} catch (...) {
		return false;
	}

	return directory.IsReallyAFile()
		? info.IsRegular()
		: info.IsDirectory();
}

static StorageFileInfo
GetDirectoryChildInfo(Storage &storage, const Directory &directory,
		      std::string_view name_utf8)
{
	const auto uri_utf8 = PathTraitsUTF8::Build(directory.GetPath(),
						    name_utf8);
	return storage.GetInfo(uri_utf8.c_str(), true);
}

bool
directory_child_is_regular(Storage &storage, const Directory &directory,
			   std::string_view name_utf8) noexcept
try {
	return GetDirectoryChildInfo(storage, directory, name_utf8)
		.IsRegular();
} catch (...) {
	return false;
}

bool
directory_child_access(const Storage &storage, const Directory &directory,
		       std::string_view name, int mode) noexcept
{
#ifdef _WIN32
	/* CheckAccess() is useless on WIN32 */
	(void)storage;
	(void)directory;
	(void)name;
	(void)mode;
	return true;
#else
	const auto path = storage.MapChildFS(directory.GetPath(), name);
	if (path.IsNull())
		/* does not point to local file: silently ignore the
		   check */
		return true;

	return CheckAccess(path, mode) || errno != EACCES;
#endif
}
