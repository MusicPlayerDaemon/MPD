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

#ifndef MPD_SPLIT_STRING_HXX
#define MPD_SPLIT_STRING_HXX

#include "Compiler.h"

#include <assert.h>

/**
 * Split a given constant string at a separator character.  Duplicates
 * the first part to be able to null-terminate it.
 */
class SplitString {
	char *first;
	const char *second;

public:
	SplitString(const char *s, char separator);

	~SplitString() {
		delete[] first;
	}

	/**
	 * Was the separator found?
	 */
	bool IsDefined() const {
		return first != nullptr;
	}

	/**
	 * Is the first part empty?
	 */
	bool IsEmpty() const {
		assert(IsDefined());

		return *first == 0;
	}

	const char *GetFirst() const {
		assert(IsDefined());

		return first;
	}

	const char *GetSecond() const {
		assert(IsDefined());

		return second;
	}
};

#endif
