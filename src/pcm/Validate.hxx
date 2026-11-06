// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

static constexpr unsigned MAX_CHANNELS = 8;

/**
 * Checks whether the number of channels is valid.
 */
constexpr bool
audio_valid_channel_count(unsigned channels) noexcept
{
	return channels >= 1 && channels <= MAX_CHANNELS;
}

/**
 * Checks whether the sample rate is valid.
 *
 * @param sample_rate the sample rate in Hz
 */
constexpr bool
audio_valid_sample_rate(unsigned sample_rate) noexcept
{
	return sample_rate > 0 && sample_rate < (1 << 30);
}
