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

#ifndef MPD_ICU_COMPARE_HXX
#define MPD_ICU_COMPARE_HXX

#include "util/AllocatedString.hxx"

#include <string_view>

#ifdef _WIN32
#include <wchar.h>
#endif

/**
 * This class can compare one string ("needle") with lots of other
 * strings ("haystacks") efficiently, ignoring case.  With some
 * configurations, it can prepare a case-folded version of the needle.
 */
class IcuCompare {
#ifdef _WIN32
	/* Windows API functions work with wchar_t strings, so let's
	   cache the MultiByteToWideChar() result for performance */
	using AllocatedString = BasicAllocatedString<wchar_t>;
#endif

	AllocatedString needle;

public:
	IcuCompare():needle(nullptr) {}

	explicit IcuCompare(std::string_view needle) noexcept;

	IcuCompare(const IcuCompare &src) noexcept
		:needle(src
			? AllocatedString(src.needle)
			: nullptr) {}

	IcuCompare &operator=(const IcuCompare &src) noexcept {
		needle = src
			? AllocatedString(src.needle)
			: nullptr;
		return *this;
	}

	IcuCompare(IcuCompare &&) = default;
	IcuCompare &operator=(IcuCompare &&) = default;

	[[gnu::pure]]
	operator bool() const noexcept {
		return needle != nullptr;
	}

	[[gnu::pure]]
	bool operator==(const char *haystack) const noexcept;

	[[gnu::pure]]
	bool IsIn(const char *haystack) const noexcept;
};

#endif
