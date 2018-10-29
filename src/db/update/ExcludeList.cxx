/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "input/TextInputStream.hxx"
#include "util/StringUtil.hxx"
#include "Log.hxx"

#include <stdexcept>

#include <assert.h>
#include <string.h>

inline void
ExcludeList::ParseLine(char *line) noexcept
{
	char *p = Strip(line);
	if (*p != 0 && *p != '#')
		patterns.emplace_front(p);
}

bool
ExcludeList::Load(InputStreamPtr is)
{
#ifdef HAVE_CLASS_GLOB
	TextInputStream tis(std::move(is));

	char *line;
	while ((line = tis.ReadLine()) != nullptr)
		ParseLine(line);
#else
	/* not implemented */
	(void)path_fs;
#endif

	return true;
}

bool
ExcludeList::Check(Path name_fs) const noexcept
{
	assert(!name_fs.IsNull());

	/* XXX include full path name in check */

#ifdef HAVE_CLASS_GLOB
	if (parent != nullptr) {
		if (parent->Check(name_fs)) {
			return true;
		}
	}

	for (const auto &i : patterns) {
		try {
			if (i.Check(NarrowPath(name_fs).c_str()))
				return true;
		} catch (...) {
		}
	}
#else
	/* not implemented */
	(void)name_fs;
#endif

	return false;
}
