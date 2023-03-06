// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
