/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "GlueResampler.hxx"
#include "ConfiguredResampler.hxx"
#include "Resampler.hxx"

#include <assert.h>

GluePcmResampler::GluePcmResampler()
	:resampler(pcm_resampler_create()) {}

GluePcmResampler::~GluePcmResampler()
{
	delete resampler;
}

bool
GluePcmResampler::Open(AudioFormat src_format, unsigned new_sample_rate,
		       Error &error)
{
	assert(src_format.IsValid());
	assert(audio_valid_sample_rate(new_sample_rate));

	AudioFormat requested_format = src_format;
	AudioFormat dest_format = resampler->Open(requested_format,
						  new_sample_rate,
						  error);
	if (!dest_format.IsValid())
		return false;

	assert(requested_format.channels == src_format.channels);
	assert(dest_format.channels == src_format.channels);
	assert(dest_format.sample_rate == new_sample_rate);

	if (requested_format.format != src_format.format &&
	    !format_converter.Open(src_format.format, requested_format.format,
				   error))
		return false;

	src_sample_format = src_format.format;
	requested_sample_format = requested_format.format;
	output_sample_format = dest_format.format;
	return true;
}

void
GluePcmResampler::Close()
{
	if (requested_sample_format != src_sample_format)
		format_converter.Close();

	resampler->Close();
}

ConstBuffer<void>
GluePcmResampler::Resample(ConstBuffer<void> src, Error &error)
{
	assert(!src.IsNull());

	if (requested_sample_format != src_sample_format) {
		src = format_converter.Convert(src, error);
		if (src.IsNull())
			return nullptr;
	}

	return resampler->Resample(src, error);
}
