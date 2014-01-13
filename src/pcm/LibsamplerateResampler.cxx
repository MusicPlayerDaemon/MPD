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
#include "LibsamplerateResampler.hxx"
#include "util/ASCII.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static constexpr Domain libsamplerate_domain("libsamplerate");

static int lsr_converter = SRC_SINC_FASTEST;

static bool
lsr_parse_converter(const char *s)
{
	assert(s != nullptr);

	if (*s == 0)
		return true;

	char *endptr;
	long l = strtol(s, &endptr, 10);
	if (*endptr == 0 && src_get_name(l) != nullptr) {
		lsr_converter = l;
		return true;
	}

	size_t length = strlen(s);
	for (int i = 0;; ++i) {
		const char *name = src_get_name(i);
		if (name == nullptr)
			break;

		if (StringEqualsCaseASCII(s, name, length)) {
			lsr_converter = i;
			return true;
		}
	}

	return false;
}

bool
pcm_resample_lsr_global_init(const char *converter, Error &error)
{
	if (!lsr_parse_converter(converter)) {
		error.Format(libsamplerate_domain,
			     "unknown samplerate converter '%s'", converter);
		return false;
	}

	FormatDebug(libsamplerate_domain,
		    "libsamplerate converter '%s'",
		    src_get_name(lsr_converter));

	return true;
}

AudioFormat
LibsampleratePcmResampler::Open(AudioFormat &af, unsigned new_sample_rate,
				Error &error)
{
	assert(af.IsValid());
	assert(audio_valid_sample_rate(new_sample_rate));

	src_rate = af.sample_rate;
	dest_rate = new_sample_rate;
	channels = af.channels;

	/* libsamplerate works with floating point samples */
	af.format = SampleFormat::FLOAT;

	int src_error;
	state = src_new(lsr_converter, channels, &src_error);
	if (!state) {
		error.Format(libsamplerate_domain, src_error,
			     "libsamplerate initialization has failed: %s",
			     src_strerror(src_error));
		return AudioFormat::Undefined();
	}

	memset(&data, 0, sizeof(data));

	data.src_ratio = double(new_sample_rate) / double(af.sample_rate);
	FormatDebug(libsamplerate_domain,
		    "setting samplerate conversion ratio to %.2lf",
		    data.src_ratio);
	src_set_ratio(state, data.src_ratio);

	AudioFormat result = af;
	result.sample_rate = new_sample_rate;
	return result;
}

void
LibsampleratePcmResampler::Close()
{
	state = src_delete(state);
}

static bool
src_process(SRC_STATE *state, SRC_DATA *data, Error &error)
{
	int result = src_process(state, data);
	if (result != 0) {
		error.Format(libsamplerate_domain, result,
			     "libsamplerate has failed: %s",
			     src_strerror(result));
		return false;
	}

	return true;
}

inline ConstBuffer<float>
LibsampleratePcmResampler::Resample2(ConstBuffer<float> src, Error &error)
{
	assert(src.size % channels == 0);

	const unsigned src_frames = src.size / channels;
	const unsigned dest_frames =
		(src_frames * dest_rate + src_rate - 1) / src_rate;
	size_t data_out_size = dest_frames * sizeof(float) * channels;

	data.data_in = const_cast<float *>(src.data);
	data.data_out = (float *)buffer.Get(data_out_size);
	data.input_frames = src_frames;
	data.output_frames = dest_frames;

	if (!src_process(state, &data, error))
		return nullptr;

	return ConstBuffer<float>(data.data_out,
				  data.output_frames_gen * channels);
}

ConstBuffer<void>
LibsampleratePcmResampler::Resample(ConstBuffer<void> src, Error &error)
{
	return Resample2(ConstBuffer<float>::FromVoid(src), error).ToVoid();
}
