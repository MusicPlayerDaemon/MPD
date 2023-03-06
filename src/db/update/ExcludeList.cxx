// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * The .mpdignore backend code.
 *
 */

#include "ExcludeList.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "input/TextInputStream.hxx"
#include "util/StringStrip.hxx"
#include "config.h"

#include <cassert>

#ifdef HAVE_CLASS_GLOB

inline void
ExcludeList::ParseLine(char *line) noexcept
{
	char *p = Strip(line);
	if (*p != 0 && *p != '#')
		patterns.emplace_front(p);
}

#endif

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
	(void)is;
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
