/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h" /* must be first for large file support */
#include "UpdateIO.hxx"
#include "UpdateDomain.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "storage/FileInfo.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/Traits.hxx"
#include "fs/FileSystem.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <errno.h>
#include <unistd.h>

bool
GetInfo(Storage &storage, const char *uri_utf8, FileInfo &info)
{
	Error error;
	bool success = storage.GetInfo(uri_utf8, true, info, error);
	if (!success)
		LogError(error);
	return success;
}

bool
GetInfo(StorageDirectoryReader &reader, FileInfo &info)
{
	Error error;
	bool success = reader.GetInfo(true, info, error);
	if (!success)
		LogError(error);
	return success;
}

bool
DirectoryExists(Storage &storage, const Directory &directory)
{
	FileInfo info;
	if (!storage.GetInfo(directory.GetPath(), true, info, IgnoreError()))
		return false;

	return directory.device == DEVICE_INARCHIVE ||
		directory.device == DEVICE_CONTAINER
		? info.IsRegular()
		: info.IsDirectory();
}

static bool
GetDirectoryChildInfo(Storage &storage, const Directory &directory,
		      const char *name_utf8, FileInfo &info, Error &error)
{
	const auto uri_utf8 = PathTraitsUTF8::Build(directory.GetPath(),
						    name_utf8);
	return storage.GetInfo(uri_utf8.c_str(), true, info, error);
}

bool
directory_child_is_regular(Storage &storage, const Directory &directory,
			   const char *name_utf8)
{
	FileInfo info;
	return GetDirectoryChildInfo(storage, directory, name_utf8, info,
				     IgnoreError()) &&
		info.IsRegular();
}

bool
directory_child_access(Storage &storage, const Directory &directory,
		       const char *name, int mode)
{
#ifdef WIN32
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
