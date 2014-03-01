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

#ifndef MPD_EXCLUDE_H
#define MPD_EXCLUDE_H

#include "check.h"
#include "Compiler.h"

#include <forward_list>

#ifdef HAVE_GLIB
#include <glib.h>
#endif

class Path;

class ExcludeList {
#ifdef HAVE_GLIB
	class Pattern {
		GPatternSpec *pattern;

	public:
		Pattern(const char *_pattern)
			:pattern(g_pattern_spec_new(_pattern)) {}

		Pattern(Pattern &&other)
			:pattern(other.pattern) {
			other.pattern = nullptr;
		}

		~Pattern() {
			g_pattern_spec_free(pattern);
		}

		gcc_pure
		bool Check(const char *name_fs) const {
			return g_pattern_match_string(pattern, name_fs);
		}
	};

	std::forward_list<Pattern> patterns;
#else
	// TODO: implement
#endif

public:
	gcc_pure
	bool IsEmpty() const {
#ifdef HAVE_GLIB
		return patterns.empty();
#else
		// TODO: implement
		return true;
#endif
	}

	/**
	 * Loads and parses a .mpdignore file.
	 */
	bool LoadFile(Path path_fs);

	/**
	 * Checks whether one of the patterns in the .mpdignore file matches
	 * the specified file name.
	 */
	bool Check(Path name_fs) const;
};


#endif
