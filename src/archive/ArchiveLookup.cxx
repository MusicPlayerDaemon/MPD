/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "ArchiveDomain.hxx"
#include "Log.hxx"
#include "fs/FileInfo.hxx"
#include "system/Error.hxx"

#include <string.h>

gcc_pure
static char *
FindSlash(char *p, size_t i) noexcept
{
	for (; i > 0; --i)
		if (p[i] == '/')
			return p + i;

	return nullptr;
}

gcc_pure
static const char *
FindSuffix(const char *p, const char *i) noexcept
{
	for (; i > p; --i) {
		if (*i == '.')
			return i + 1;
	}

	return nullptr;
}

bool
archive_lookup(char *pathname, const char **archive,
	       const char **inpath, const char **suffix)
{
	size_t idx = strlen(pathname);

	char *slash = nullptr;

	while (true) {
		try {
			//try to stat if its real directory
			const FileInfo file_info(Path::FromFS(pathname));

			//is something found ins original path (is not an archive)
			if (slash == nullptr)
				return false;

			//its a file ?
			if (file_info.IsRegular()) {
				//so the upper should be file
				*archive = pathname;
				*inpath = slash + 1;

				//try to get suffix
				*suffix = FindSuffix(pathname, slash - 1);
				return true;
			} else {
				FormatError(archive_domain,
					    "Not a regular file: %s",
					    pathname);
				return false;
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
			return false;

		*slash = 0;
		idx = slash - pathname;
	}
}

