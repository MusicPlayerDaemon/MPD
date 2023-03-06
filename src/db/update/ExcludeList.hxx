// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
