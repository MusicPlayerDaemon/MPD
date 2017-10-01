/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "Collate.hxx"
#include "util/AllocatedString.hxx"

#ifdef HAVE_ICU
#include "Util.hxx"
#include "util/RuntimeError.hxx"

#include <unicode/ucol.h>
#include <unicode/ustring.h>
#else
#include <algorithm>
#include <ctype.h>
#endif

#ifdef WIN32
#include "Win32.hxx"
#include "util/AllocatedString.hxx"
#include <windows.h>
#endif

#include <memory>
#include <stdexcept>

#include <assert.h>
#include <string.h>

#ifdef HAVE_ICU
static UCollator *collator;
#endif

#ifdef HAVE_ICU

void
IcuCollateInit()
{
	assert(collator == nullptr);

	UErrorCode code = U_ZERO_ERROR;
	collator = ucol_open("", &code);
	if (collator == nullptr)
		throw FormatRuntimeError("ucol_open() failed: %s",
					 u_errorName(code));
}

void
IcuCollateFinish() noexcept
{
	assert(collator != nullptr);

	ucol_close(collator);
}

#endif

gcc_pure
int
IcuCollate(const char *a, const char *b) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(a != nullptr);
	assert(b != nullptr);
#endif

#ifdef HAVE_ICU
	assert(collator != nullptr);

#if U_ICU_VERSION_MAJOR_NUM >= 50
	UErrorCode code = U_ZERO_ERROR;
	return (int)ucol_strcollUTF8(collator, a, -1, b, -1, &code);
#else
	/* fall back to ucol_strcoll() */

	try {
		const auto au = UCharFromUTF8(a);
		const auto bu = UCharFromUTF8(b);

		return ucol_strcoll(collator, au.begin(), au.size(),
				    bu.begin(), bu.size());
	} catch (const std::runtime_error &) {
		/* fall back to plain strcasecmp() */
		return strcasecmp(a, b);
	}
#endif

#elif defined(WIN32)
	AllocatedString<wchar_t> wa = nullptr, wb = nullptr;

	try {
		wa = MultiByteToWideChar(CP_UTF8, a);
	} catch (const std::runtime_error &) {
		try {
			wb = MultiByteToWideChar(CP_UTF8, b);
			return -1;
		} catch (const std::runtime_error &) {
			return 0;
		}
	}

	try {
		wb = MultiByteToWideChar(CP_UTF8, b);
	} catch (const std::runtime_error &) {
		return 1;
	}

	auto result = CompareStringEx(LOCALE_NAME_INVARIANT,
				      LINGUISTIC_IGNORECASE,
				      wa.c_str(), -1,
				      wb.c_str(), -1,
				      nullptr, nullptr, 0);
	if (result != 0)
		/* "To maintain the C runtime convention of comparing
		   strings, the value 2 can be subtracted from a
		   nonzero return value." */
		result -= 2;

	return result;
#else
	return strcoll(a, b);
#endif
}
