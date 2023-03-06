// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_SILENCE_HXX
#define MPD_PCM_SILENCE_HXX

#include <cstddef>
#include <cstdint>
#include <span>

enum class SampleFormat : uint8_t;

/**
 * Fill the given buffer with the format-specific silence pattern.
 */
void
PcmSilence(std::span<std::byte> dest, SampleFormat format) noexcept;

#endif
