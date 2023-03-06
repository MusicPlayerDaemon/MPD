// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_SOXR_RESAMPLER_HXX
#define MPD_PCM_SOXR_RESAMPLER_HXX

#include "Resampler.hxx"
#include "Buffer.hxx"

struct AudioFormat;
struct ConfigBlock;

/**
 * A resampler using soxr.
 */
class SoxrPcmResampler final : public PcmResampler {
	struct soxr *soxr;

	unsigned channels;
	float ratio;

	PcmBuffer buffer;

public:
	AudioFormat Open(AudioFormat &af, unsigned new_sample_rate) override;
	void Close() noexcept override;
	void Reset() noexcept override;
	std::span<const std::byte> Resample(std::span<const std::byte> src) override;
	std::span<const std::byte> Flush() override;
};

void
pcm_resample_soxr_global_init(const ConfigBlock &block);

#endif
