// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_LIBSAMPLERATE_RESAMPLER_HXX
#define MPD_PCM_LIBSAMPLERATE_RESAMPLER_HXX

#include "Resampler.hxx"
#include "Buffer.hxx"
#include "AudioFormat.hxx"

#include <samplerate.h>

struct ConfigBlock;

/**
 * A resampler using libsamplerate.
 */
class LibsampleratePcmResampler final : public PcmResampler {
	unsigned src_rate, dest_rate;
	unsigned channels;

	SRC_STATE *state;
	SRC_DATA data;

	PcmBuffer buffer;

public:
	AudioFormat Open(AudioFormat &af, unsigned new_sample_rate) override;
	void Close() noexcept override;
	void Reset() noexcept override;
	std::span<const std::byte> Resample(std::span<const std::byte> src) override;

private:
	std::span<const float> Resample2(std::span<const float> src);
};

void
pcm_resample_lsr_global_init(const ConfigBlock &block);

#endif
