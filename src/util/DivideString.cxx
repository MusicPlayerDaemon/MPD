// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DivideString.hxx"
#include "StringStrip.hxx"

#include <cstring>

DivideString::DivideString(const char *s, char separator, bool strip) noexcept
	:first(nullptr)
{
	const char *x = std::strchr(s, separator);
	if (x == nullptr)
		return;

	size_t length = x - s;
	second = x + 1;

	if (strip)
		second = StripLeft(second);

	if (strip) {
		const char *end = s + length;
		s = StripLeft(s);
		end = StripRight(s, end);
		length = end - s;
	}

	first = new char[length + 1];
	memcpy(first, s, length);
	first[length] = 0;
}
