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
#include <ctype.h>
#endif

#ifdef _WIN32
#include "Win32.hxx"
#include <windows.h>
#endif

#include <memory>

#include <assert.h>
#include <string.h>

AllocatedString<>
IcuCaseFold(const char *src) noexcept
try {
#ifdef HAVE_ICU
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(src != nullptr);
#endif

	const auto u = UCharFromUTF8(src);
	if (u.IsNull())
		return AllocatedString<>::Duplicate(src);

	AllocatedArray<UChar> folded(u.size() * 2u);

	UErrorCode error_code = U_ZERO_ERROR;
	size_t folded_length = u_strFoldCase(folded.begin(), folded.size(),
					     u.begin(), u.size(),
					     U_FOLD_CASE_DEFAULT,
					     &error_code);
	if (folded_length == 0 || error_code != U_ZERO_ERROR)
		return AllocatedString<>::Duplicate(src);

	folded.SetSize(folded_length);
	return UCharToUTF8({folded.begin(), folded.size()});

#elif defined(_WIN32)
	const auto u = MultiByteToWideChar(CP_UTF8, src);

	const int size = LCMapStringEx(LOCALE_NAME_INVARIANT,
				       LCMAP_SORTKEY|LINGUISTIC_IGNORECASE,
				       u.c_str(), -1, nullptr, 0,
				       nullptr, nullptr, 0);
	if (size <= 0)
		return AllocatedString<>::Duplicate(src);

	std::unique_ptr<wchar_t[]> buffer(new wchar_t[size]);
	if (LCMapStringEx(LOCALE_NAME_INVARIANT,
			  LCMAP_SORTKEY|LINGUISTIC_IGNORECASE,
			  u.c_str(), -1, buffer.get(), size,
			  nullptr, nullptr, 0) <= 0)
		return AllocatedString<>::Duplicate(src);

	return WideCharToMultiByte(CP_UTF8, buffer.get());

#else
#error not implemented
#endif
} catch (...) {
	return AllocatedString<>::Duplicate(src);
}

#endif /* HAVE_ICU_CASE_FOLD */
