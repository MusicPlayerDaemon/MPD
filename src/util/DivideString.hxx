// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DIVIDE_STRING_HXX
#define MPD_DIVIDE_STRING_HXX

#include <cassert>

/**
 * Split a given constant string at a separator character.  Duplicates
 * the first part to be able to null-terminate it.
 */
class DivideString {
	char *first;
	const char *second;

public:
	/**
	 * @param strip strip the first part and left-strip the second
	 * part?
	 */
	DivideString(const char *s, char separator, bool strip=false) noexcept;

	~DivideString() {
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
	bool empty() const {
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
