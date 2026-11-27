// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <chrono>
#include <cstddef>
#include <string_view>

/**
 * Throws on error.
 */
bool
ParseBool(const char *value);

/**
 * Throws on error.
 */
unsigned
ParseUnsigned(std::string_view s);

/**
 * Throws on error.
 */
unsigned
ParsePositive(std::string_view s);

/**
 * Throws on error.
 */
double
ParseDouble(const char *s);

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
