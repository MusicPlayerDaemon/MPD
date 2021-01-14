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

#ifndef MPD_ICU_UTIL_HXX
#define MPD_ICU_UTIL_HXX

#include <unicode/utypes.h>

#include <string_view>

template<typename T> class AllocatedArray;
class AllocatedString;

/**
 * Wrapper for u_strFromUTF8().
 *
 * Throws std::runtime_error on error.
 */
AllocatedArray<UChar>
UCharFromUTF8(std::string_view src);

/**
 * Wrapper for u_strToUTF8().
 *
 * Throws std::runtime_error on error.
 */
AllocatedString
UCharToUTF8(std::basic_string_view<UChar> src);

#endif
