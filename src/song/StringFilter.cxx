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

#include "StringFilter.hxx"
#include "util/StringAPI.hxx"

#include <cassert>

bool
StringFilter::MatchWithoutNegation(const char *s) const noexcept
{
	assert(s != nullptr);

#ifdef HAVE_PCRE
	if (regex)
		return regex->Match(s);
#endif

	if (fold_case) {
		return substring
			? fold_case.IsIn(s)
			: fold_case == s;
	} else {
		return substring
			? StringFind(s, value.c_str()) != nullptr
			: value == s;
	}
}

bool
StringFilter::Match(const char *s) const noexcept
{
	return MatchWithoutNegation(s) != negated;
}
