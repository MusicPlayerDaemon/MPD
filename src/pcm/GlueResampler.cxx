/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "PcmConvert.hxx"
#include "PcmFormat.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"

bool
GluePcmResampler::Open(AudioFormat _src_format, unsigned _new_sample_rate,
		       gcc_unused Error &error)
{
	src_format = _src_format;
	new_sample_rate = _new_sample_rate;

	return true;
}

void
GluePcmResampler::Close()
{
	resampler.Reset();
}

ConstBuffer<void>
GluePcmResampler::Resample(ConstBuffer<void> src, Error &error)
{
	const void *result;
	size_t size;

	switch (src_format.format) {
	case SampleFormat::S16:
		result = resampler.Resample16(src_format.channels,
					      src_format.sample_rate,
					      (const int16_t *)src.data,
					      src.size,
					      new_sample_rate, &size,
					      error);
		break;

	case SampleFormat::S24_P32:
		result = resampler.Resample24(src_format.channels,
					      src_format.sample_rate,
					      (const int32_t *)src.data,
					      src.size,
					      new_sample_rate, &size,
					      error);
		break;

	case SampleFormat::S32:
		result = resampler.Resample24(src_format.channels,
					      src_format.sample_rate,
					      (const int32_t *)src.data,
					      src.size,
					      new_sample_rate, &size,
					      error);
		break;

	case SampleFormat::FLOAT:
		result = resampler.ResampleFloat(src_format.channels,
						 src_format.sample_rate,
						 (const float *)src.data,
						 src.size,
						 new_sample_rate, &size,
						 error);
		break;

	default:
		error.Format(pcm_convert_domain,
			     "Resampling %s is not implemented",
			     sample_format_to_string(src_format.format));
		return nullptr;
	}

	return { result, size };
}
