// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SINGLE_MODE_HXX
#define MPD_SINGLE_MODE_HXX

#include <cstdint>

enum class SingleMode : uint8_t {
	OFF,
	ON,
	ONE_SHOT,
};

/**
 * Return the string representation of a #SingleMode.
 */
[[gnu::const]]
const char *
SingleToString(SingleMode mode) noexcept;

/**
 * Parse a string to a #SingleMode.  Throws std::invalid_argument on error.
 */
SingleMode
SingleFromString(const char *s);

#endif
