/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
	ConstBuffer<void> Resample(ConstBuffer<void> src) override;
	ConstBuffer<void> Flush() override;
};

void
pcm_resample_soxr_global_init(const ConfigBlock &block);

#endif
