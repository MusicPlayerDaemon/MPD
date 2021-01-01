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
