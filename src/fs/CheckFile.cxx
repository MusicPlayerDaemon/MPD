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

#include "config.h"
#include "CheckFile.hxx"
#include "Log.hxx"
#include "config/ConfigError.hxx"
#include "FileSystem.hxx"
#include "Path.hxx"
#include "AllocatedPath.hxx"
#include "DirectoryReader.hxx"

#include <errno.h>
#include <sys/stat.h>

void
CheckDirectoryReadable(Path path_fs)
{
	struct stat st;
	if (!StatFile(path_fs, st)) {
		FormatErrno(config_domain,
			    "Failed to stat directory \"%s\"",
			    path_fs.c_str());
		return;
	}

	if (!S_ISDIR(st.st_mode)) {
		FormatError(config_domain,
			    "Not a directory: %s", path_fs.c_str());
		return;
	}

#ifndef WIN32
	const auto x = AllocatedPath::Build(path_fs, ".");
	if (!StatFile(x, st) && errno == EACCES)
		FormatError(config_domain,
			    "No permission to traverse (\"execute\") directory: %s",
			    path_fs.c_str());
#endif

	const DirectoryReader reader(path_fs);
	if (reader.HasFailed() && errno == EACCES)
		FormatError(config_domain,
			    "No permission to read directory: %s", path_fs.c_str());

}
