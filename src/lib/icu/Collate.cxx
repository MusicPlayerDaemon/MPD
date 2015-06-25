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

#include "config.h"
#include "Collate.hxx"
#include "util/AllocatedString.hxx"

#ifdef HAVE_ICU
#include "Util.hxx"
#include "Error.hxx"
#include "util/WritableBuffer.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <unicode/ucol.h>
#include <unicode/ustring.h>
#elif defined(HAVE_GLIB)
#include <glib.h>
#else
#include <algorithm>
#include <ctype.h>
#endif

#ifdef WIN32
#include "Win32.hxx"
#include "util/AllocatedString.hxx"
#include <windows.h>
#endif

#include <assert.h>
#include <string.h>
#include <strings.h>

#ifdef HAVE_ICU
static UCollator *collator;
#endif

#ifdef HAVE_ICU

bool
IcuCollateInit(Error &error)
{
	assert(collator == nullptr);
	assert(!error.IsDefined());

	UErrorCode code = U_ZERO_ERROR;
	collator = ucol_open("", &code);
	if (collator == nullptr) {
		error.Format(icu_domain, int(code),
			     "ucol_open() failed: %s", u_errorName(code));
		return false;
	}

	return true;
}

void
IcuCollateFinish()
{
	assert(collator != nullptr);

	ucol_close(collator);
}

#endif

gcc_pure
int
IcuCollate(const char *a, const char *b)
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

	const auto au = UCharFromUTF8(a);
	const auto bu = UCharFromUTF8(b);

	int result = !au.IsNull() && !bu.IsNull()
		? (int)ucol_strcoll(collator, au.data, au.size,
				    bu.data, bu.size)
		: strcasecmp(a, b);

	delete[] au.data;
	delete[] bu.data;

	return result;
#endif

#elif defined(WIN32)
	const auto wa = MultiByteToWideChar(CP_UTF8, a);
	const auto wb = MultiByteToWideChar(CP_UTF8, b);
	if (wa.IsNull())
		return wb.IsNull() ? 0 : -1;
	else if (wb.IsNull())
		return 1;

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
#elif defined(HAVE_GLIB)
	return g_utf8_collate(a, b);
#else
	return strcoll(a, b);
#endif
}

AllocatedString<>
IcuCaseFold(const char *src)
{
#ifdef HAVE_ICU
	assert(collator != nullptr);
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(src != nullptr);
#endif

	const auto u = UCharFromUTF8(src);
	if (u.IsNull())
		return AllocatedString<>::Duplicate(src);

	size_t folded_capacity = u.size * 2u;
	UChar *folded = new UChar[folded_capacity];

	UErrorCode error_code = U_ZERO_ERROR;
	size_t folded_length = u_strFoldCase(folded, folded_capacity,
					     u.data, u.size,
					     U_FOLD_CASE_DEFAULT,
					     &error_code);
	delete[] u.data;
	if (folded_length == 0 || error_code != U_ZERO_ERROR) {
		delete[] folded;
		return AllocatedString<>::Duplicate(src);
	}

	auto result = UCharToUTF8({folded, folded_length});
	delete[] folded;
	return result;
#elif defined(HAVE_GLIB)
	char *tmp = g_utf8_casefold(src, -1);
	auto result = AllocatedString<>::Duplicate(tmp);
	g_free(tmp);
	return result;
#else
	size_t size = strlen(src) + 1;
	auto buffer = new char[size];
	size_t nbytes = strxfrm(buffer, src, size);
	if (nbytes >= size) {
		/* buffer too small - reallocate and try again */
		delete[] buffer;
		size = nbytes + 1;
		buffer = new char[size];
		nbytes = strxfrm(buffer, src, size);
	}

	assert(nbytes < size);
	assert(buffer[nbytes] == 0);

	return AllocatedString<>::Donate(buffer);
#endif
}

