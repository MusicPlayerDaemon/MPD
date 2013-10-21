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
#include "PcmResampleInternal.hxx"
#include "util/ASCII.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int lsr_converter = SRC_SINC_FASTEST;

static constexpr Domain libsamplerate_domain("libsamplerate");

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

void
pcm_resample_lsr_init(PcmResampler *state)
{
	state->state = nullptr;
	memset(&state->data, 0, sizeof(state->data));
	memset(&state->prev, 0, sizeof(state->prev));
	state->error = 0;
}

void
pcm_resample_lsr_deinit(PcmResampler *state)
{
	if (state->state != nullptr)
		state->state = src_delete(state->state);
}

void
pcm_resample_lsr_reset(PcmResampler *state)
{
	if (state->state != nullptr)
		src_reset(state->state);
}

static bool
pcm_resample_set(PcmResampler *state,
		 unsigned channels, unsigned src_rate, unsigned dest_rate,
		 Error &error_r)
{
	/* (re)set the state/ratio if the in or out format changed */
	if (channels == state->prev.channels &&
	    src_rate == state->prev.src_rate &&
	    dest_rate == state->prev.dest_rate)
		return true;

	state->error = 0;
	state->prev.channels = channels;
	state->prev.src_rate = src_rate;
	state->prev.dest_rate = dest_rate;

	if (state->state)
		state->state = src_delete(state->state);

	int error;
	state->state = src_new(lsr_converter, channels, &error);
	if (!state->state) {
		error_r.Format(libsamplerate_domain, error,
			       "libsamplerate initialization has failed: %s",
			       src_strerror(error));
		return false;
	}

	SRC_DATA *data = &state->data;
	data->src_ratio = (double)dest_rate / (double)src_rate;
	FormatDebug(libsamplerate_domain,
		    "setting samplerate conversion ratio to %.2lf",
		    data->src_ratio);
	src_set_ratio(state->state, data->src_ratio);

	return true;
}

static bool
lsr_process(PcmResampler *state, Error &error)
{
	if (state->error == 0)
		state->error = src_process(state->state, &state->data);
	if (state->error) {
		error.Format(libsamplerate_domain, state->error,
			     "libsamplerate has failed: %s",
			     src_strerror(state->error));
		return false;
	}

	return true;
}

const float *
pcm_resample_lsr_float(PcmResampler *state,
		       unsigned channels,
		       unsigned src_rate,
		       const float *src_buffer, size_t src_size,
		       unsigned dest_rate, size_t *dest_size_r,
		       Error &error)
{
	SRC_DATA *data = &state->data;

	assert((src_size % (sizeof(*src_buffer) * channels)) == 0);

	if (!pcm_resample_set(state, channels, src_rate, dest_rate, error))
		return nullptr;

	data->input_frames = src_size / sizeof(*src_buffer) / channels;
	data->data_in = const_cast<float *>(src_buffer);

	data->output_frames = (src_size * dest_rate + src_rate - 1) / src_rate;
	size_t data_out_size = data->output_frames * sizeof(float) * channels;
	data->data_out = (float *)state->out.Get(data_out_size);

	if (!lsr_process(state, error))
		return nullptr;

	*dest_size_r = data->output_frames_gen *
		sizeof(*data->data_out) * channels;
	return data->data_out;
}

const int16_t *
pcm_resample_lsr_16(PcmResampler *state,
		    unsigned channels,
		    unsigned src_rate,
		    const int16_t *src_buffer, size_t src_size,
		    unsigned dest_rate, size_t *dest_size_r,
		    Error &error)
{
	SRC_DATA *data = &state->data;

	assert((src_size % (sizeof(*src_buffer) * channels)) == 0);

	if (!pcm_resample_set(state, channels, src_rate, dest_rate,
			      error))
		return nullptr;

	data->input_frames = src_size / sizeof(*src_buffer) / channels;
	size_t data_in_size = data->input_frames * sizeof(float) * channels;
	data->data_in = (float *)state->in.Get(data_in_size);

	data->output_frames = (src_size * dest_rate + src_rate - 1) / src_rate;
	size_t data_out_size = data->output_frames * sizeof(float) * channels;
	data->data_out = (float *)state->out.Get(data_out_size);

	src_short_to_float_array(src_buffer, data->data_in,
				 data->input_frames * channels);

	if (!lsr_process(state, error))
		return nullptr;

	int16_t *dest_buffer;
	*dest_size_r = data->output_frames_gen *
		sizeof(*dest_buffer) * channels;
	dest_buffer = (int16_t *)state->buffer.Get(*dest_size_r);
	src_float_to_short_array(data->data_out, dest_buffer,
				 data->output_frames_gen * channels);

	return dest_buffer;
}

#ifdef HAVE_LIBSAMPLERATE_NOINT

/* libsamplerate introduced these functions in v0.1.3 */

static void
src_int_to_float_array(const int *in, float *out, int len)
{
	while (len-- > 0)
		*out++ = *in++ / (float)(1 << (24 - 1));
}

static void
src_float_to_int_array (const float *in, int *out, int len)
{
	while (len-- > 0)
		*out++ = *in++ * (float)(1 << (24 - 1));
}

#endif

const int32_t *
pcm_resample_lsr_32(PcmResampler *state,
		    unsigned channels,
		    unsigned src_rate,
		    const int32_t *src_buffer, size_t src_size,
		    unsigned dest_rate, size_t *dest_size_r,
		    Error &error)
{
	SRC_DATA *data = &state->data;

	assert((src_size % (sizeof(*src_buffer) * channels)) == 0);

	if (!pcm_resample_set(state, channels, src_rate, dest_rate,
			      error))
		return nullptr;

	data->input_frames = src_size / sizeof(*src_buffer) / channels;
	size_t data_in_size = data->input_frames * sizeof(float) * channels;
	data->data_in = (float *)state->in.Get(data_in_size);

	data->output_frames = (src_size * dest_rate + src_rate - 1) / src_rate;
	size_t data_out_size = data->output_frames * sizeof(float) * channels;
	data->data_out = (float *)state->out.Get(data_out_size);

	src_int_to_float_array(src_buffer, data->data_in,
			       data->input_frames * channels);

	if (!lsr_process(state, error))
		return nullptr;

	int32_t *dest_buffer;
	*dest_size_r = data->output_frames_gen *
		sizeof(*dest_buffer) * channels;
	dest_buffer = (int32_t *)state->buffer.Get(*dest_size_r);
	src_float_to_int_array(data->data_out, dest_buffer,
			       data->output_frames_gen * channels);

	return dest_buffer;
}
