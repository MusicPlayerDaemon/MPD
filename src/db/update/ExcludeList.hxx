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

/*
 * The .mpdignore backend code.
 *
 */

#ifndef MPD_EXCLUDE_H
#define MPD_EXCLUDE_H

#include "fs/Glob.hxx"
#include "input/Ptr.hxx"
#include "config.h"

#ifdef HAVE_CLASS_GLOB
#include <forward_list>
#endif

class Path;

class ExcludeList {
	const ExcludeList *const parent;

#ifdef HAVE_CLASS_GLOB
	std::forward_list<Glob> patterns;
#endif

public:
	ExcludeList() noexcept
		:parent(nullptr) {}

	ExcludeList(const ExcludeList &_parent) noexcept
		:parent(&_parent) {}

	[[gnu::pure]]
	bool IsEmpty() const noexcept {
#ifdef HAVE_CLASS_GLOB
		return ((parent == nullptr) || parent->IsEmpty()) && patterns.empty();
#else
		/* not implemented */
		return true;
#endif
	}

	/**
	 * Loads and parses a .mpdignore file.
	 *
	 * Throws on I/O error.
	 */
	bool Load(InputStreamPtr is);

	/**
	 * Checks whether one of the patterns in the .mpdignore file matches
	 * the specified file name.
	 */
	bool Check(Path name_fs) const noexcept;

private:
	void ParseLine(char *line) noexcept;
};


#endif
