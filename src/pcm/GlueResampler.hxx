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

#ifndef MPD_GLUE_RESAMPLER_HXX
#define MPD_GLUE_RESAMPLER_HXX

#include "SampleFormat.hxx"
#include "FormatConverter.hxx"

struct AudioFormat;
class PcmResampler;
template<typename T> struct ConstBuffer;

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

	ConstBuffer<void> Resample(ConstBuffer<void> src);

	ConstBuffer<void> Flush();
};

#endif
