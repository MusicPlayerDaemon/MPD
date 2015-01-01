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

#ifndef MPD_ICU_CONVERTER_HXX
#define MPD_ICU_CONVERTER_HXX

#include "check.h"
#include "Compiler.h"

#ifdef HAVE_ICU
#include "thread/Mutex.hxx"
#define HAVE_ICU_CONVERTER
#elif defined(HAVE_GLIB)
#include <glib.h>
#define HAVE_ICU_CONVERTER
#endif

#ifdef HAVE_ICU_CONVERTER

#include <string>

class Error;

#ifdef HAVE_ICU
struct UConverter;
#endif

/**
 * This class can convert strings with a certain character set to and
 * from UTF-8.
 */
class IcuConverter {
#ifdef HAVE_ICU
	/**
	 * ICU's UConverter class is not thread-safe.  This mutex
	 * serializes simultaneous calls.
	 */
	mutable Mutex mutex;

	UConverter *const converter;

	IcuConverter(UConverter *_converter):converter(_converter) {}
#elif defined(HAVE_GLIB)
	const GIConv to_utf8, from_utf8;

	IcuConverter(GIConv _to, GIConv _from)
		:to_utf8(_to), from_utf8(_from) {}
#endif

public:
#ifdef HAVE_ICU
	~IcuConverter();
#elif defined(HAVE_GLIB)
	~IcuConverter() {
		g_iconv_close(to_utf8);
		g_iconv_close(from_utf8);
	}
#endif

	static IcuConverter *Create(const char *charset, Error &error);

	/**
	 * Convert the string to UTF-8.
	 * Returns empty string on error.
	 */
	gcc_pure gcc_nonnull_all
	std::string ToUTF8(const char *s) const;

	/**
	 * Convert the string from UTF-8.
	 * Returns empty string on error.
	 */
	gcc_pure gcc_nonnull_all
	std::string FromUTF8(const char *s) const;
};

#endif

#endif
