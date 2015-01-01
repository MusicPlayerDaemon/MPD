/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "SplitString.hxx"
#include "StringUtil.hxx"

#include <string.h>

std::forward_list<std::string>
SplitString(const char *s, char separator, bool strip)
{
	if (strip)
		s = StripLeft(s);

	std::forward_list<std::string> list;
	if (*s == 0)
		return list;

	auto i = list.before_begin();

	while (true) {
		const char *next = strchr(s, separator);
		if (next == nullptr)
			break;

		const char *end = next++;
		if (strip)
			end = StripRight(s, end);

		i = list.emplace_after(i, s, end);

		s = next;
		if (strip)
			s = StripLeft(s);
	}

	const char *end = s + strlen(s);
	if (strip)
		end = StripRight(s, end);

	list.emplace_after(i, s, end);
	return list;
}
