// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

	[[gnu::pure]]
	bool StartsWith(const char *haystack) const noexcept;
};

#endif
