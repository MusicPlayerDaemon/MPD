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

/*
 * The .mpdignore backend code.
 *
 */

#include "config.h"
#include "ExcludeList.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/io/TextFile.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_GLIB

gcc_pure
static bool
IsFileNotFound(const Error &error)
{
#ifdef WIN32
	return error.IsDomain(win32_domain) &&
		error.GetCode() == ERROR_FILE_NOT_FOUND;
#else
	return error.IsDomain(errno_domain) && error.GetCode() == ENOENT;
#endif
}

#endif

bool
ExcludeList::LoadFile(Path path_fs)
{
#ifdef HAVE_GLIB
	Error error;
	TextFile file(path_fs, error);
	if (file.HasFailed()) {
		if (!IsFileNotFound(error))
			LogError(error);
		return false;
	}

	char *line;
	while ((line = file.ReadLine()) != nullptr) {
		char *p = strchr(line, '#');
		if (p != nullptr)
			*p = 0;

		p = Strip(line);
		if (*p != 0)
			patterns.emplace_front(p);
	}
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
		if (i.Check(NarrowPath(name_fs).c_str()))
			return true;
#else
	// TODO: implement
	(void)name_fs;
#endif

	return false;
}
