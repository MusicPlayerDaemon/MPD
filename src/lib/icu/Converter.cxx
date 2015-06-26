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
#include "Converter.hxx"
#include "Error.hxx"
#include "util/Error.hxx"
#include "util/Macros.hxx"
#include "util/AllocatedString.hxx"
#include "util/WritableBuffer.hxx"
#include "util/ConstBuffer.hxx"

#include <string.h>

#ifdef HAVE_ICU
#include "Util.hxx"
#include <unicode/ucnv.h>
#elif defined(HAVE_GLIB)
#include "util/Domain.hxx"
static constexpr Domain g_iconv_domain("g_iconv");
#endif

#ifdef HAVE_ICU

IcuConverter::~IcuConverter()
{
	ucnv_close(converter);
}

#endif

#ifdef HAVE_ICU_CONVERTER

IcuConverter *
IcuConverter::Create(const char *charset, Error &error)
{
#ifdef HAVE_ICU
	UErrorCode code = U_ZERO_ERROR;
	UConverter *converter = ucnv_open(charset, &code);
	if (converter == nullptr) {
		error.Format(icu_domain, int(code),
			     "Failed to initialize charset '%s': %s",
			     charset, u_errorName(code));
		return nullptr;
	}

	return new IcuConverter(converter);
#elif defined(HAVE_GLIB)
	GIConv to = g_iconv_open("utf-8", charset);
	GIConv from = g_iconv_open(charset, "utf-8");
	if (to == (GIConv)-1 || from == (GIConv)-1) {
		if (to != (GIConv)-1)
			g_iconv_close(to);
		if (from != (GIConv)-1)
			g_iconv_close(from);
		error.Format(g_iconv_domain,
			     "Failed to initialize charset '%s'", charset);
		return nullptr;
	}

	return new IcuConverter(to, from);
#endif
}

#ifdef HAVE_ICU
#elif defined(HAVE_GLIB)

static AllocatedString<char>
DoConvert(GIConv conv, const char *src)
{
	// TODO: dynamic buffer?
	char buffer[4096];
	char *in = const_cast<char *>(src);
	char *out = buffer;
	size_t in_left = strlen(src);
	size_t out_left = sizeof(buffer);

	size_t n = g_iconv(conv, &in, &in_left, &out, &out_left);

	if (n == static_cast<size_t>(-1) || in_left > 0)
		return nullptr;

	return AllocatedString<>::Duplicate(buffer, sizeof(buffer) - out_left);
}

#endif

AllocatedString<char>
IcuConverter::ToUTF8(const char *s) const
{
#ifdef HAVE_ICU
	const ScopeLock protect(mutex);

	ucnv_resetToUnicode(converter);

	// TODO: dynamic buffer?
	UChar buffer[4096], *target = buffer;
	const char *source = s;

	UErrorCode code = U_ZERO_ERROR;

	ucnv_toUnicode(converter, &target, buffer + ARRAY_SIZE(buffer),
		       &source, source + strlen(source),
		       nullptr, true, &code);
	if (code != U_ZERO_ERROR)
		return nullptr;

	const size_t target_length = target - buffer;
	return UCharToUTF8({buffer, target_length});
#elif defined(HAVE_GLIB)
	return DoConvert(to_utf8, s);
#endif
}

AllocatedString<char>
IcuConverter::FromUTF8(const char *s) const
{
#ifdef HAVE_ICU
	const ScopeLock protect(mutex);

	const auto u = UCharFromUTF8(s);
	if (u.IsNull())
		return nullptr;

	ucnv_resetFromUnicode(converter);

	// TODO: dynamic buffer?
	char buffer[4096], *target = buffer;
	const UChar *source = u.data;
	UErrorCode code = U_ZERO_ERROR;

	ucnv_fromUnicode(converter, &target, buffer + ARRAY_SIZE(buffer),
			 &source, u.end(),
			 nullptr, true, &code);
	delete[] u.data;

	if (code != U_ZERO_ERROR)
		return nullptr;

	return AllocatedString<>::Duplicate(buffer, target);

#elif defined(HAVE_GLIB)
	return DoConvert(from_utf8, s);
#endif
}

#endif
