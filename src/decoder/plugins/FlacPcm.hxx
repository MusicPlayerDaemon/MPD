// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FLAC_PCM_HXX
#define MPD_FLAC_PCM_HXX

#include "pcm/Buffer.hxx"
#include "pcm/AudioFormat.hxx"

#include <FLAC/ordinals.h>

#include <cstddef>
#include <span>

/**
 * This class imports libFLAC PCM data into a PCM format supported by
 * MPD.
 */
class FlacPcmImport {
	PcmBuffer buffer;

	AudioFormat audio_format;

public:
	/**
	 * Throws #std::runtime_error on error.
	 */
	void Open(unsigned sample_rate, unsigned bits_per_sample,
		  unsigned channels);

	const AudioFormat &GetAudioFormat() const noexcept {
		return audio_format;
	}

	std::span<const std::byte> Import(const FLAC__int32 *const src[],
					  size_t n_frames) noexcept;
};

#endif
