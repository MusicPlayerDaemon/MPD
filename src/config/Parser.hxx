// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
