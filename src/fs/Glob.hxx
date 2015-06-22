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

#ifndef MPD_FS_GLOB_XX
#define MPD_FS_GLOB_XX

#include "check.h"

#ifdef HAVE_GLIB
#define HAVE_CLASS_GLOB
#include <glib.h>
#endif

#ifdef HAVE_CLASS_GLOB
#include "Compiler.h"

/**
 * A pattern that matches file names.  It may contain shell wildcards
 * (asterisk and question mark).
 */
class Glob {
	GPatternSpec *pattern;

public:
	explicit Glob(const char *_pattern)
		:pattern(g_pattern_spec_new(_pattern)) {}

	Glob(Glob &&other)
		:pattern(other.pattern) {
		other.pattern = nullptr;
	}

	~Glob() {
		g_pattern_spec_free(pattern);
	}

	gcc_pure
	bool Check(const char *name_fs) const {
		return g_pattern_match_string(pattern, name_fs);
	}
};

#endif

#endif
