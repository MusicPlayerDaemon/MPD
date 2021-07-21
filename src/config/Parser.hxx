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

#ifndef MPD_CONFIG_PARSER_HXX
#define MPD_CONFIG_PARSER_HXX

#include <chrono>
#include <cstddef>

/**
 * Throws on error.
 */
bool
ParseBool(const char *value);

/**
 * Throws on error.
 */
long
ParseLong(const char *s);

/**
 * Throws on error.
 */
unsigned
ParseUnsigned(const char *s);

/**
 * Throws on error.
 */
unsigned
ParsePositive(const char *s);

/**
 * Parse a string as a byte size.
 *
 * Throws on error.
 */
std::size_t
ParseSize(const char *s, std::size_t default_factor=1);

/**
 * Throws on error.
 */
std::chrono::steady_clock::duration
ParseDuration(const char *s);

#endif
