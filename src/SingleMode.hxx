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

#ifndef MPD_SINGLE_MODE_HXX
#define MPD_SINGLE_MODE_HXX

#include "Compiler.h"

#include <stdint.h>

enum class SingleMode : uint8_t {
	OFF,
	ON,
	ONE_SHOT,
};

/**
 * Return the string representation of a #SingleMode.
 */
gcc_pure
const char *
SingleToString(SingleMode mode) noexcept;

/**
 * Parse a string to a #SingleMode.  Throws std::runtime_error on error.
 */
SingleMode
SingleFromString(const char *s);

// #define SingleToString ToString
// #define SingleFromString FromString
#endif
