// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_CHANNEL_DEFS_HXX
#define MPD_PCM_CHANNEL_DEFS_HXX

static constexpr unsigned MAX_CHANNELS = 8;

/**
 * Checks whether the number of channels is valid.
 */
constexpr bool
audio_valid_channel_count(unsigned channels) noexcept
{
	return channels >= 1 && channels <= MAX_CHANNELS;
}

#endif
