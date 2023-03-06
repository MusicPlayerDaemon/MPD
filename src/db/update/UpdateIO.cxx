// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
