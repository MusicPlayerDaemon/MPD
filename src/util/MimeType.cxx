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

#include "MimeType.hxx"
#include "SplitString.hxx"

#include <string.h>

std::string
GetMimeTypeBase(const char *s) noexcept
{
	const char *semicolon = strchr(s, ';');
	return semicolon != nullptr
		? std::string(s, semicolon)
		: std::string(s);
}

std::map<std::string, std::string>
ParseMimeTypeParameters(const char *s) noexcept
{
	std::map<std::string, std::string> result;

	auto l = SplitString(s, ';', true);
	if (!l.empty())
		l.pop_front();

	for (const auto &i : l) {
		const auto eq = i.find('=');
		if (eq == i.npos)
			continue;

		result.insert(std::make_pair(std::string(&i.front(), eq),
					     std::string(&i[eq + 1])));
	}

	return result;
}
