/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "src/UpdateDomain.hxx"
#include "Directory.hxx"
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "Log.hxx"

#include <errno.h>
#include <unistd.h>

int
stat_directory(const Directory *directory, struct stat *st)
{
	const auto path_fs = map_directory_fs(directory);
	if (path_fs.IsNull())
		return -1;

	if (!StatFile(path_fs, *st)) {
		int error = errno;
		const std::string path_utf8 = path_fs.ToUTF8();
		FormatErrno(update_domain, error,
			    "Failed to stat %s", path_utf8.c_str());
		return -1;
	}

	return 0;
}

int
stat_directory_child(const Directory *parent, const char *name,
		     struct stat *st)
{
	const auto path_fs = map_directory_child_fs(parent, name);
	if (path_fs.IsNull())
		return -1;

	if (!StatFile(path_fs, *st)) {
		int error = errno;
		const std::string path_utf8 = path_fs.ToUTF8();
		FormatErrno(update_domain, error,
			    "Failed to stat %s", path_utf8.c_str());
		return -1;
	}

	return 0;
}

bool
directory_exists(const Directory *directory)
{
	const auto path_fs = map_directory_fs(directory);
	if (path_fs.IsNull())
		/* invalid path: cannot exist */
		return false;

	return directory->device == DEVICE_INARCHIVE ||
		directory->device == DEVICE_CONTAINER
		? FileExists(path_fs)
		: DirectoryExists(path_fs);
}

bool
directory_child_is_regular(const Directory *directory,
			   const char *name_utf8)
{
	const auto path_fs = map_directory_child_fs(directory, name_utf8);
	if (path_fs.IsNull())
		return false;

	return FileExists(path_fs);
}

bool
directory_child_access(const Directory *directory,
		       const char *name, int mode)
{
#ifdef WIN32
	/* CheckAccess() is useless on WIN32 */
	(void)directory;
	(void)name;
	(void)mode;
	return true;
#else
	const auto path = map_directory_child_fs(directory, name);
	if (path.IsNull())
		/* something went wrong, but that isn't a permission
		   problem */
		return true;

	return CheckAccess(path, mode) || errno != EACCES;
#endif
}
