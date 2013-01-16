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
#include "Directory.hxx"
#include "Mapper.hxx"
#include "Path.hxx"
#include "glib_compat.h"

#include <glib.h>

#include <errno.h>
#include <unistd.h>

int
stat_directory(const Directory *directory, struct stat *st)
{
	const Path path_fs = map_directory_fs(directory);
	if (path_fs.IsNull())
		return -1;

	int ret = stat(path_fs.c_str(), st);
	if (ret < 0)
		g_warning("Failed to stat %s: %s",
			  path_fs.c_str(), g_strerror(errno));

	return ret;
}

int
stat_directory_child(const Directory *parent, const char *name,
		     struct stat *st)
{
	const Path path_fs = map_directory_child_fs(parent, name);
	if (path_fs.IsNull())
		return -1;

	int ret = stat(path_fs.c_str(), st);
	if (ret < 0)
		g_warning("Failed to stat %s: %s",
			  path_fs.c_str(), g_strerror(errno));

	return ret;
}

bool
directory_exists(const Directory *directory)
{
	const Path path_fs = map_directory_fs(directory);
	if (path_fs.IsNull())
		/* invalid path: cannot exist */
		return false;

	GFileTest test = directory->device == DEVICE_INARCHIVE ||
		directory->device == DEVICE_CONTAINER
		? G_FILE_TEST_IS_REGULAR
		: G_FILE_TEST_IS_DIR;

	return g_file_test(path_fs.c_str(), test);
}

bool
directory_child_is_regular(const Directory *directory,
			   const char *name_utf8)
{
	const Path path_fs = map_directory_child_fs(directory, name_utf8);
	if (path_fs.IsNull())
		return false;

	struct stat st;
	return stat(path_fs.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool
directory_child_access(const Directory *directory,
		       const char *name, int mode)
{
#ifdef WIN32
	/* access() is useless on WIN32 */
	(void)directory;
	(void)name;
	(void)mode;
	return true;
#else
	const Path path = map_directory_child_fs(directory, name);
	if (path.IsNull())
		/* something went wrong, but that isn't a permission
		   problem */
		return true;

	return access(path.c_str(), mode) == 0 || errno != EACCES;
#endif
}
