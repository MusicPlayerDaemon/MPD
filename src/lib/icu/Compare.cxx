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

#include "Compare.hxx"
#include "CaseFold.hxx"
#include "util/StringAPI.hxx"
#include "config.h"

#ifdef _WIN32
#include "Win32.hxx"
#include <windows.h>
#endif

#ifdef HAVE_ICU_CASE_FOLD

IcuCompare::IcuCompare(std::string_view _needle) noexcept
	:needle(IcuCaseFold(_needle)) {}

#elif defined(_WIN32)

IcuCompare::IcuCompare(std::string_view _needle) noexcept
	:needle(nullptr)
{
	try {
		needle = MultiByteToWideChar(CP_UTF8, _needle);
	} catch (...) {
	}
}

#else

IcuCompare::IcuCompare(std::string_view _needle) noexcept
	:needle(_needle) {}

#endif

bool
IcuCompare::operator==(const char *haystack) const noexcept
{
#ifdef HAVE_ICU_CASE_FOLD
	return StringIsEqual(IcuCaseFold(haystack).c_str(), needle.c_str());
#elif defined(_WIN32)
	if (needle == nullptr)
		/* the MultiByteToWideChar() call in the constructor
		   has failed, so let's always fail the comparison */
		return false;

	try {
		auto w_haystack = MultiByteToWideChar(CP_UTF8, haystack);
		return CompareStringEx(LOCALE_NAME_INVARIANT,
				       NORM_IGNORECASE,
				       w_haystack.c_str(), -1,
				       needle.c_str(), -1,
				       nullptr, nullptr, 0) == CSTR_EQUAL;
	} catch (...) {
		return false;
	}
#else
	return StringIsEqualIgnoreCase(haystack, needle.c_str());
#endif
}

bool
IcuCompare::IsIn(const char *haystack) const noexcept
{
#ifdef HAVE_ICU_CASE_FOLD
	return StringFind(IcuCaseFold(haystack).c_str(),
			  needle.c_str()) != nullptr;
#elif defined(_WIN32)
	if (needle == nullptr)
		/* the MultiByteToWideChar() call in the constructor
		   has failed, so let's always fail the comparison */
		return false;

	try {
		auto w_haystack = MultiByteToWideChar(CP_UTF8, haystack);
		return FindNLSStringEx(LOCALE_NAME_INVARIANT,
				       FIND_FROMSTART|NORM_IGNORECASE,
				       w_haystack.c_str(), -1,
				       needle.c_str(), -1,
				       nullptr,
				       nullptr, nullptr, 0) >= 0;
	} catch (...) {
		/* MultiByteToWideChar() has failed */
		return false;
	}
#elif defined(HAVE_STRCASESTR)
	return strcasestr(haystack, needle.c_str()) != nullptr;
#else
	/* poor man's strcasestr() */
	for (const size_t length = strlen(needle.c_str());
	     *haystack != 0; ++haystack)
		if (strncasecmp(haystack, needle.c_str(), length) == 0)
			return true;

	return false;
#endif
}
