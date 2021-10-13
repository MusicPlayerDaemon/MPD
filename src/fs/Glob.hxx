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

#ifndef MPD_FS_GLOB_XX
#define MPD_FS_GLOB_XX

#include "config.h"

#ifdef HAVE_FNMATCH
#define HAVE_CLASS_GLOB
#include <fnmatch.h>
#elif defined(_WIN32)
#define HAVE_CLASS_GLOB
#endif

#ifdef HAVE_CLASS_GLOB
#include <string>

/**
 * A pattern that matches file names.  It may contain shell wildcards
 * (asterisk and question mark).
 */
class Glob {
	std::string pattern;

public:
	explicit Glob(const char *_pattern)
		:pattern(_pattern) {}

	Glob(Glob &&other) noexcept = default;
	Glob &operator=(Glob &&other) noexcept = default;

	[[gnu::pure]]
	bool Check(const char *name_fs) const noexcept;
};

#ifdef HAVE_FNMATCH

inline bool
Glob::Check(const char *name_fs) const noexcept
{
	return fnmatch(pattern.c_str(), name_fs, 0) == 0;
}

#endif

#endif /* HAVE_CLASS_GLOB */

#endif
