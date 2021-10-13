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

#ifndef MPD_ICU_CONVERTER_HXX
#define MPD_ICU_CONVERTER_HXX

#include "config.h"

#ifdef HAVE_ICU
#include "thread/Mutex.hxx"
#define HAVE_ICU_CONVERTER
#elif defined(HAVE_ICONV)
#include <iconv.h>
#define HAVE_ICU_CONVERTER
#endif

#ifdef HAVE_ICU_CONVERTER

#include <memory>
#include <string_view>

#ifdef HAVE_ICU
struct UConverter;
#endif

class AllocatedString;

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
#elif defined(HAVE_ICONV)
	const iconv_t to_utf8, from_utf8;

	IcuConverter(iconv_t _to, iconv_t _from)
		:to_utf8(_to), from_utf8(_from) {}
#endif

public:
#ifdef HAVE_ICU
	~IcuConverter();
#elif defined(HAVE_ICONV)
	~IcuConverter() {
		iconv_close(to_utf8);
		iconv_close(from_utf8);
	}
#endif

	/**
	 * Throws std::runtime_error on error.
	 */
	static std::unique_ptr<IcuConverter> Create(const char *charset);

	/**
	 * Convert the string to UTF-8.
	 *
	 * Throws std::runtime_error on error.
	 */
	AllocatedString ToUTF8(std::string_view s) const;

	/**
	 * Convert the string from UTF-8.
	 *
	 * Throws std::runtime_error on error.
	 */
	AllocatedString FromUTF8(std::string_view s) const;
};

#endif

#endif
