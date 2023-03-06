// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

std::span<const std::byte>
GluePcmResampler::Resample(std::span<const std::byte> src)
{
	if (requested_sample_format != src_sample_format)
		src = format_converter.Convert(src);

	return resampler->Resample(src);
}

std::span<const std::byte>
GluePcmResampler::Flush()
{
	return resampler->Flush();
}
