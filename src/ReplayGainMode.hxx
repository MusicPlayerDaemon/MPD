// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_REPLAY_GAIN_MODE_HXX
#define MPD_REPLAY_GAIN_MODE_HXX

#include <cstdint>

enum class ReplayGainMode : uint8_t {
	OFF,
	ALBUM,
	TRACK,
	AUTO,
};

/**
 * Return the string representation of a #ReplayGainMode.
 */
[[gnu::pure]]
const char *
ToString(ReplayGainMode mode) noexcept;

/**
 * Parse a string to a #ReplayGainMode.  Throws std::runtime_error on
 * error.
 */
ReplayGainMode
FromString(const char *s);

#endif
