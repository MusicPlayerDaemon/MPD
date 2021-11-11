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

#include "CaseFold.hxx"
#include "config.h"

#ifdef HAVE_ICU_CASE_FOLD

#include "util/AllocatedString.hxx"

#ifdef HAVE_ICU
#include "Util.hxx"
#include "util/AllocatedArray.hxx"

#include <unicode/ucol.h>
#include <unicode/ustring.h>
#else
#include <algorithm>
#endif

#include <memory>

#include <string.h>

AllocatedString
IcuCaseFold(std::string_view src) noexcept
try {
#ifdef HAVE_ICU
	const auto u = UCharFromUTF8(src);
	if (u.IsNull())
		return {src};

	AllocatedArray<UChar> folded(u.size() * 2U);

	UErrorCode error_code = U_ZERO_ERROR;
	size_t folded_length = u_strFoldCase(folded.begin(), folded.size(),
					     u.begin(), u.size(),
					     U_FOLD_CASE_DEFAULT,
					     &error_code);
	if (folded_length == 0 || error_code != U_ZERO_ERROR)
		return {src};

	folded.SetSize(folded_length);
	return UCharToUTF8({folded.begin(), folded.size()});

#else
#error not implemented
#endif
} catch (...) {
	return {src};
}

#endif /* HAVE_ICU_CASE_FOLD */
