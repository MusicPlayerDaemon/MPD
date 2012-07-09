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
#include "update_io.h"
#include "mapper.h"
#include "directory.h"
#include "glib_compat.h"

#include <glib.h>

#include <errno.h>
#include <unistd.h>

int
stat_directory(const struct directory *directory, struct stat *st)
{
	char *path_fs = map_directory_fs(directory);
	if (path_fs == NULL)
		return -1;

	int ret = stat(path_fs, st);
	if (ret < 0)
		g_warning("Failed to stat %s: %s", path_fs, g_strerror(errno));

	g_free(path_fs);
	return ret;
}

int
stat_directory_child(const struct directory *parent, const char *name,
		     struct stat *st)
{
	char *path_fs = map_directory_child_fs(parent, name);
	if (path_fs == NULL)
		return -1;

	int ret = stat(path_fs, st);
	if (ret < 0)
		g_warning("Failed to stat %s: %s", path_fs, g_strerror(errno));

	g_free(path_fs);
	return ret;
}

bool
directory_exists(const struct directory *directory)
{
	char *path_fs = map_directory_fs(directory);
	if (path_fs == NULL)
		/* invalid path: cannot exist */
		return false;

	GFileTest test = directory->device == DEVICE_INARCHIVE ||
		directory->device == DEVICE_CONTAINER
		? G_FILE_TEST_IS_REGULAR
		: G_FILE_TEST_IS_DIR;

	bool exists = g_file_test(path_fs, test);
	g_free(path_fs);

	return exists;
}

bool
directory_child_is_regular(const struct directory *directory,
			   const char *name_utf8)
{
	char *path_fs = map_directory_child_fs(directory, name_utf8);
	if (path_fs == NULL)
		return false;

	struct stat st;
	bool is_regular = stat(path_fs, &st) == 0 && S_ISREG(st.st_mode);
	g_free(path_fs);

	return is_regular;
}

bool
directory_child_access(const struct directory *directory,
		       const char *name, int mode)
{
#ifdef WIN32
	/* access() is useless on WIN32 */
	(void)directory;
	(void)name;
	(void)mode;
	return true;
#else
	char *path = map_directory_child_fs(directory, name);
	if (path == NULL)
		/* something went wrong, but that isn't a permission
		   problem */
		return true;

	bool success = access(path, mode) == 0 || errno != EACCES;
	g_free(path);
	return success;
#endif
}
