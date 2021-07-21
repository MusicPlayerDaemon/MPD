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

#include "Util.hxx"

/** Get rid of white space at both ends */
void
trimstring(std::string &s, const char *ws) noexcept
{
	auto pos = s.find_first_not_of(ws);
	if (pos == std::string::npos) {
		s.clear();
		return;
	}
	s.erase(0, pos);

	pos = s.find_last_not_of(ws);
	if (pos != std::string::npos && pos != s.length()-1)
		s.erase(pos + 1);
}

static void
path_catslash(std::string &s) noexcept
{
	if (s.empty() || s.back() != '/')
		s += '/';
}

std::string
path_getfather(const std::string &s) noexcept
{
	std::string father = s;

	// ??
	if (father.empty())
		return "./";

	if (father.back() == '/') {
		// Input ends with /. Strip it, handle special case for root
		if (father.length() == 1)
			return father;
		father.erase(father.length()-1);
	}

	auto slp = father.rfind('/');
	if (slp == std::string::npos)
		return "./";

	father.erase(slp);
	path_catslash(father);
	return father;
}
