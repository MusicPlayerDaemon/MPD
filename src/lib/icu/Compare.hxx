/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_ICU_COMPARE_HXX
#define MPD_ICU_COMPARE_HXX

#include "util/Compiler.h"
#include "util/AllocatedString.hxx"

/**
 * This class can compare one string ("needle") with lots of other
 * strings ("haystacks") efficiently, ignoring case.  With some
 * configurations, it can prepare a case-folded version of the needle.
 */
class IcuCompare {
	AllocatedString<> needle;

public:
	IcuCompare():needle(nullptr) {}

	explicit IcuCompare(const char *needle) noexcept;

	IcuCompare(const IcuCompare &src) noexcept
		:needle(src
			? AllocatedString<>::Duplicate(src.needle.c_str())
			: nullptr) {}

	IcuCompare &operator=(const IcuCompare &src) noexcept {
		needle = src
			? AllocatedString<>::Duplicate(src.needle.c_str())
			: nullptr;
		return *this;
	}

	IcuCompare(IcuCompare &&) = default;
	IcuCompare &operator=(IcuCompare &&) = default;

	gcc_pure
	operator bool() const noexcept {
		return !needle.IsNull();
	}

	gcc_pure
	bool operator==(const char *haystack) const noexcept;

	gcc_pure
	bool IsIn(const char *haystack) const noexcept;
};

#endif
