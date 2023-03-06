// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_ALSA_VERSION_HXX
#define MPD_ALSA_VERSION_HXX

#include <cstdint>

static constexpr uint_least32_t
MakeAlsaVersion(uint_least32_t major, uint_least32_t minor,
		uint_least32_t subminor) noexcept
{
	return (major << 16) | (minor << 8) | subminor;
}

/**
 * Wrapper for snd_asoundlib_version() which translates the resulting
 * string to an integer constructed with MakeAlsaVersion().
 */
[[gnu::const]]
uint_least32_t
GetRuntimeAlsaVersion() noexcept;

#endif
