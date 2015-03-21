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

#ifndef WSTRING_UTIL_HXX
#define WSTRING_UTIL_HXX

#include "Compiler.h"

#include <wchar.h>

gcc_pure
bool
StringStartsWith(const wchar_t *haystack, const wchar_t *needle);

gcc_pure
bool
StringEndsWith(const wchar_t *haystack, const wchar_t *needle);

/**
 * Check if the given string ends with the specified suffix.  If yes,
 * returns the position of the suffix, and nullptr otherwise.
 */
gcc_pure
const wchar_t *
FindStringSuffix(const wchar_t *p, const wchar_t *suffix);

#endif
