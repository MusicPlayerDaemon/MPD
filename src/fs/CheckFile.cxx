/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "FileInfo.hxx"
#include "Path.hxx"
#include "AllocatedPath.hxx"
#include "DirectoryReader.hxx"
#include "system/Error.hxx"

#include <errno.h>
#include <sys/stat.h>

void
CheckDirectoryReadable(Path path_fs)
{
	Error error;

	FileInfo fi;
	if (!GetFileInfo(path_fs, fi, error)) {
		LogError(error);
		return;
	}

	if (!fi.IsDirectory()) {
		const auto path_utf8 = path_fs.ToUTF8();
		FormatError(config_domain,
			    "Not a directory: %s", path_utf8.c_str());
		return;
	}

#ifndef WIN32
	const auto x = AllocatedPath::Build(path_fs,
					    PathTraitsFS::CURRENT_DIRECTORY);
	if (!GetFileInfo(x, fi) && errno == EACCES) {
		const auto path_utf8 = path_fs.ToUTF8();
		FormatError(config_domain,
			    "No permission to traverse (\"execute\") directory: %s",
			    path_utf8.c_str());
	}
#endif

	try {
		const DirectoryReader reader(path_fs);
	} catch (const std::system_error &e) {
		if (IsAccessDenied(e))
			FormatError(config_domain,
				    "No permission to read directory: %s",
				    path_fs.ToUTF8().c_str());
	}
}
