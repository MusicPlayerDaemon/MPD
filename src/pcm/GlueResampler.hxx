// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_GLUE_RESAMPLER_HXX
#define MPD_GLUE_RESAMPLER_HXX

#include "SampleFormat.hxx"
#include "FormatConverter.hxx"

#include <cstddef>
#include <span>

struct AudioFormat;
class PcmResampler;

/**
 * A glue class that integrates a #PcmResampler and automatically
 * converts source data to the sample format required by the
 * #PcmResampler instance.
 */
class GluePcmResampler {
	PcmResampler *const resampler;

	SampleFormat src_sample_format, requested_sample_format;
	SampleFormat output_sample_format;

	/**
	 * This object converts input data to the sample format
	 * requested by the #PcmResampler.
	 */
	PcmFormatConverter format_converter;

public:
	GluePcmResampler();
	~GluePcmResampler() noexcept;

	void Open(AudioFormat src_format, unsigned new_sample_rate);
	void Close() noexcept;

	SampleFormat GetOutputSampleFormat() const noexcept {
		return output_sample_format;
	}

	/**
	 * @see PcmResampler::Reset()
	 */
	void Reset() noexcept;

	std::span<const std::byte> Resample(std::span<const std::byte> src);

	std::span<const std::byte> Flush();
};

#endif
