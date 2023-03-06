// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_FALLBACK_RESAMPLER_HXX
#define MPD_PCM_FALLBACK_RESAMPLER_HXX

#include "Resampler.hxx"
#include "Buffer.hxx"
#include "AudioFormat.hxx"

#include <cstddef>
#include <span>

/**
 * A naive resampler that is used when no external library was found
 * (or when the user explicitly asks for bad quality).
 */
class FallbackPcmResampler final : public PcmResampler {
	AudioFormat format;
	unsigned out_rate;

	PcmBuffer buffer;

public:
	AudioFormat Open(AudioFormat &af, unsigned new_sample_rate) override;
	void Close() noexcept override;
	std::span<const std::byte> Resample(std::span<const std::byte> src) override;
};

#endif
