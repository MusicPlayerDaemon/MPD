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

#include "GlueResampler.hxx"
#include "ConfiguredResampler.hxx"
#include "Resampler.hxx"
#include "AudioFormat.hxx"

#include <cassert>

GluePcmResampler::GluePcmResampler()
	:resampler(pcm_resampler_create()) {}

GluePcmResampler::~GluePcmResampler() noexcept
{
	delete resampler;
}

void
GluePcmResampler::Open(AudioFormat src_format, unsigned new_sample_rate)
{
	assert(src_format.IsValid());
	assert(audio_valid_sample_rate(new_sample_rate));

	AudioFormat requested_format = src_format;
	AudioFormat dest_format = resampler->Open(requested_format,
						  new_sample_rate);
	assert(dest_format.IsValid());

	assert(requested_format.channels == src_format.channels);
	assert(dest_format.channels == src_format.channels);
	assert(dest_format.sample_rate == new_sample_rate);

	if (requested_format.format != src_format.format)
		format_converter.Open(src_format.format,
				      requested_format.format);

	src_sample_format = src_format.format;
	requested_sample_format = requested_format.format;
	output_sample_format = dest_format.format;
}

void
GluePcmResampler::Close() noexcept
{
	if (requested_sample_format != src_sample_format)
		format_converter.Close();

	resampler->Close();
}

void
GluePcmResampler::Reset() noexcept
{
	resampler->Reset();
}

ConstBuffer<void>
GluePcmResampler::Resample(ConstBuffer<void> src)
{
	assert(!src.IsNull());

	if (requested_sample_format != src_sample_format)
		src = format_converter.Convert(src);

	return resampler->Resample(src);
}

ConstBuffer<void>
GluePcmResampler::Flush()
{
	return resampler->Flush();
}
