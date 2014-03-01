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

/*
 * The .mpdignore backend code.
 *
 */

#include "config.h"
#include "ExcludeList.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"
#include "util/StringUtil.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>
#include <errno.h>

static constexpr Domain exclude_list_domain("exclude_list");

bool
ExcludeList::LoadFile(Path path_fs)
{
#ifdef HAVE_GLIB
	FILE *file = FOpen(path_fs, FOpenMode::ReadText);
	if (file == nullptr) {
		const int e = errno;
		if (e != ENOENT) {
			const auto path_utf8 = path_fs.ToUTF8();
			FormatErrno(exclude_list_domain,
				    "Failed to open %s",
				    path_utf8.c_str());
		}

		return false;
	}

	char line[1024];
	while (fgets(line, sizeof(line), file) != nullptr) {
		char *p = strchr(line, '#');
		if (p != nullptr)
			*p = 0;

		p = Strip(line);
		if (*p != 0)
			patterns.emplace_front(p);
	}

	fclose(file);
#else
	// TODO: implement
	(void)path_fs;
#endif

	return true;
}

bool
ExcludeList::Check(Path name_fs) const
{
	assert(!name_fs.IsNull());

	/* XXX include full path name in check */

#ifdef HAVE_GLIB
	for (const auto &i : patterns)
		if (i.Check(name_fs.c_str()))
			return true;
#else
	// TODO: implement
	(void)name_fs;
#endif

	return false;
}
