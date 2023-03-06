// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
