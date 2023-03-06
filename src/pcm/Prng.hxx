// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_PRNG_HXX
#define MPD_PCM_PRNG_HXX

/**
 * A very simple linear congruential PRNG.  It's good enough for PCM
 * dithering.
 */
constexpr static inline unsigned long
pcm_prng(unsigned long state) noexcept
{
	return (state * 0x0019660dL + 0x3c6ef35fL) & 0xffffffffL;
}

#endif
