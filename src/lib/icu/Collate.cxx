/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifdef HAVE_ICU
#include "Error.hxx"
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

#include <assert.h>
#include <string.h>

#ifdef HAVE_ICU
static UCollator *collator;
#endif

bool
IcuCollateInit(Error &error)
{
#ifdef HAVE_ICU
	assert(collator == nullptr);
	assert(!error.IsDefined());

	UErrorCode code;
	collator = ucol_open("", &code);
	if (collator == nullptr) {
		error.Format(icu_domain, int(code),
			     "ucol_open() failed: %s", u_errorName(code));
		return false;
	}
#else
	(void)error;
#endif

	return true;
}

void
IcuCollateFinish()
{
#ifdef HAVE_ICU
	assert(collator != nullptr);

	ucol_close(collator);
#endif
}

#ifdef HAVE_ICU

static UChar *
UCharFromUTF8(const char *src, int32_t *dest_length)
{
	assert(src != nullptr);

	const size_t src_length = strlen(src);
	size_t dest_capacity = src_length + 1;
	UChar *dest = new UChar[dest_capacity];

	UErrorCode error_code;
	u_strFromUTF8(dest, dest_capacity,
		      dest_length,
		      src, src_length,
		      &error_code);
	if (U_FAILURE(error_code)) {
		delete[] dest;
		return nullptr;
	}

	return dest;
}

#endif

gcc_pure
int
IcuCollate(const char *a, const char *b)
{
	assert(a != nullptr);
	assert(b != nullptr);

#ifdef HAVE_ICU
	assert(collator != nullptr);

#if U_ICU_VERSION_MAJOR_NUM >= 50
	return (int)ucol_strcollUTF8(collator, a, -1, b, -1, nullptr);
#else
	/* fall back to ucol_strcoll() */

	UChar *au = UCharFromUTF8(a, nullptr);
	UChar *bu = UCharFromUTF8(b, nullptr);

	int result = au != nullptr && bu != nullptr
		? (int)ucol_strcoll(collator, au, -1, bu, -1)
		: strcasecmp(a, b);

	delete[] au;
	delete[] bu;

	return result;
#endif

#elif defined(HAVE_GLIB)
	return g_utf8_collate(a, b);
#else
	return strcasecmp(a, b);
#endif
}

std::string
IcuCaseFold(const char *src)
{
#ifdef HAVE_ICU
	assert(collator != nullptr);
	assert(src != nullptr);

	int32_t u_length;
	UChar *u = UCharFromUTF8(src, &u_length);
	if (u == nullptr)
		return std::string(src);

	size_t dest_length = ucol_getSortKey(collator, u, u_length,
					     nullptr, 0);
	if (dest_length == 0) {
		delete[] u;
		return std::string(src);
	}

	uint8_t *dest = new uint8_t[dest_length];
	ucol_getSortKey(collator, u, u_length,
			dest, dest_length);
	std::string result((const char *)dest);
	delete[] dest;
#elif defined(HAVE_GLIB)
	char *tmp = g_utf8_casefold(src, -1);
	std::string result(tmp);
	g_free(tmp);
#else
	std::string result(src);
	std::transform(result.begin(), result.end(), result.begin(), tolower);
#endif
	return result;
}

