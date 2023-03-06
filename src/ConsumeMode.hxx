// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CONSUME_MODE_HXX
#define MPD_CONSUME_MODE_HXX

#include <cstdint>

enum class ConsumeMode : uint8_t {
	OFF,
	ON,
	ONE_SHOT,
};

/**
 * Return the string representation of a #ConsumeMode.
 */
[[gnu::const]]
const char *
ConsumeToString(ConsumeMode mode) noexcept;

/**
 * Parse a string to a #ConsumeMode.  Throws std::invalid_argument on error.
 */
ConsumeMode
ConsumeFromString(const char *s);

#endif
