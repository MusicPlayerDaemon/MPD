/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "ArchiveLookup.hxx"
#include "fs/FileInfo.hxx"
#include "system/Error.hxx"

#include <string.h>

gcc_pure
static PathTraitsFS::pointer_type
FindSlash(PathTraitsFS::pointer_type p, size_t i) noexcept
{
	for (; i > 0; --i)
		if (p[i] == '/')
			return p + i;

	return nullptr;
}

ArchiveLookupResult
archive_lookup(PathTraitsFS::pointer_type pathname)
{
	size_t idx = strlen(pathname);

	PathTraitsFS::pointer_type slash = nullptr;

	while (true) {
		try {
			//try to stat if its real directory
			const FileInfo file_info(Path::FromFS(pathname));

			//is something found ins original path (is not an archive)
			if (slash == nullptr)
				return {};

			//its a file ?
			if (file_info.IsRegular()) {
				//so the upper should be file
				return {Path::FromFS(pathname), Path::FromFS(slash + 1)};
			} else {
				return {};
			}
		} catch (const std::system_error &e) {
			if (!IsPathNotFound(e))
				throw;
		}

		//find one dir up
		if (slash != nullptr)
			*slash = '/';

		slash = FindSlash(pathname, idx - 1);
		if (slash == nullptr)
			return {};

		*slash = 0;
		idx = slash - pathname;
	}
}

