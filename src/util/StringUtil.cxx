// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StringUtil.hxx"
#include "StringCompare.hxx"
#include "CharUtil.hxx"

#include <cassert>

bool
StringArrayContainsCase(const char *const*haystack,
			std::string_view needle) noexcept
{
	assert(haystack != nullptr);

	for (; *haystack != nullptr; ++haystack)
		if (StringIsEqualIgnoreCase(*haystack, needle))
			return true;

	return false;
}

void
ToUpperASCII(char *dest, const char *src, size_t size) noexcept
{
	assert(dest != nullptr);
	assert(src != nullptr);
	assert(size > 1);

	char *const end = dest + size - 1;

	do {
		char ch = *src++;
		if (ch == 0)
			break;

		*dest++ = ToUpperASCII(ch);
	} while (dest < end);

	*dest = 0;
}
