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

#include "CheckFile.hxx"
#include "Log.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "config/Domain.hxx"
#include "FileInfo.hxx"
#include "Path.hxx"
#include "AllocatedPath.hxx"
#include "DirectoryReader.hxx"
#include "system/Error.hxx"

void
CheckDirectoryReadable(Path path_fs)
try {
	const FileInfo fi(path_fs);
	if (!fi.IsDirectory()) {
		FmtError(config_domain, "Not a directory: {}", path_fs);
		return;
	}

#ifndef _WIN32
	try {
		const auto x = path_fs / Path::FromFS(PathTraitsFS::CURRENT_DIRECTORY);
		const FileInfo fi2(x);
	} catch (const std::system_error &e) {
		if (IsAccessDenied(e))
			FmtError(config_domain,
				 "No permission to traverse (\"execute\") directory: {}",
				 path_fs);
	}
#endif

	try {
		const DirectoryReader reader(path_fs);
	} catch (const std::system_error &e) {
		if (IsAccessDenied(e))
			FmtError(config_domain,
				 "No permission to read directory: {}",
				 path_fs);
	}
} catch (...) {
	LogError(std::current_exception());
}
