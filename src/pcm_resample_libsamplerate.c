/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "pcm_resample_internal.h"
#include "conf.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm"

static inline GQuark
libsamplerate_quark(void)
{
	return g_quark_from_static_string("libsamplerate");
}

void
pcm_resample_lsr_deinit(struct pcm_resample_state *state)
{
	if (state->state != NULL)
		state->state = src_delete(state->state);

	pcm_buffer_deinit(&state->in);
	pcm_buffer_deinit(&state->out);
	pcm_buffer_deinit(&state->buffer);
}

static int pcm_resample_get_converter(void)
{
	const char *conf = config_get_string(CONF_SAMPLERATE_CONVERTER, NULL);
	long convalgo;
	char *test;
	const char *test2;
	size_t len;

	if (!conf) {
		convalgo = SRC_SINC_FASTEST;
		goto out;
	}

	convalgo = strtol(conf, &test, 10);
	if (*test == '\0' && src_get_name(convalgo))
		goto out;

	len = strlen(conf);
	for (convalgo = 0 ; ; convalgo++) {
		test2 = src_get_name(convalgo);
		if (!test2) {
			convalgo = SRC_SINC_FASTEST;
			break;
		}
		if (g_ascii_strncasecmp(test2, conf, len) == 0)
			goto out;
	}

	g_warning("unknown samplerate converter \"%s\"", conf);
out:
	g_debug("selecting samplerate converter \"%s\"",
		src_get_name(convalgo));

	return convalgo;
}

static bool
pcm_resample_set(struct pcm_resample_state *state,
		 uint8_t channels, unsigned src_rate, unsigned dest_rate,
		 GError **error_r)
{
	static int convalgo = -1;
	int error;
	SRC_DATA *data = &state->data;

	if (convalgo < 0)
		convalgo = pcm_resample_get_converter();

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

	state->state = src_new(convalgo, channels, &error);
	if (!state->state) {
		g_set_error(error_r, libsamplerate_quark(), state->error,
			    "libsamplerate initialization has failed: %s",
			    src_strerror(error));
		return false;
	}

	data->src_ratio = (double)dest_rate / (double)src_rate;
	g_debug("setting samplerate conversion ratio to %.2lf",
		data->src_ratio);
	src_set_ratio(state->state, data->src_ratio);

	return true;
}

const int16_t *
pcm_resample_lsr_16(struct pcm_resample_state *state,
		    uint8_t channels,
		    unsigned src_rate,
		    const int16_t *src_buffer, size_t src_size,
		    unsigned dest_rate, size_t *dest_size_r,
		    GError **error_r)
{
	bool success;
	SRC_DATA *data = &state->data;
	size_t data_in_size;
	size_t data_out_size;
	int error;
	int16_t *dest_buffer;

	assert((src_size % (sizeof(*src_buffer) * channels)) == 0);

	success = pcm_resample_set(state, channels, src_rate, dest_rate,
				   error_r);
	if (!success)
		return NULL;

	/* there was an error previously, and nothing has changed */
	if (state->error) {
		g_set_error(error_r, libsamplerate_quark(), state->error,
			    "libsamplerate has failed: %s",
			    src_strerror(state->error));
		return NULL;
	}

	data->input_frames = src_size / sizeof(*src_buffer) / channels;
	data_in_size = data->input_frames * sizeof(float) * channels;
	data->data_in = pcm_buffer_get(&state->in, data_in_size);

	data->output_frames = (src_size * dest_rate + src_rate - 1) / src_rate;
	data_out_size = data->output_frames * sizeof(float) * channels;
	data->data_out = pcm_buffer_get(&state->out, data_out_size);

	src_short_to_float_array(src_buffer, data->data_in,
				 data->input_frames * channels);

	error = src_process(state->state, data);
	if (error) {
		g_set_error(error_r, libsamplerate_quark(), error,
			    "libsamplerate has failed: %s",
			    src_strerror(error));
		state->error = error;
		return NULL;
	}

	*dest_size_r = data->output_frames_gen *
		sizeof(*dest_buffer) * channels;
	dest_buffer = pcm_buffer_get(&state->buffer, *dest_size_r);
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
pcm_resample_lsr_32(struct pcm_resample_state *state,
		    uint8_t channels,
		    unsigned src_rate,
		    const int32_t *src_buffer, size_t src_size,
		    unsigned dest_rate, size_t *dest_size_r,
		    GError **error_r)
{
	bool success;
	SRC_DATA *data = &state->data;
	size_t data_in_size;
	size_t data_out_size;
	int error;
	int32_t *dest_buffer;

	assert((src_size % (sizeof(*src_buffer) * channels)) == 0);

	success = pcm_resample_set(state, channels, src_rate, dest_rate,
				   error_r);
	if (!success)
		return NULL;

	/* there was an error previously, and nothing has changed */
	if (state->error) {
		g_set_error(error_r, libsamplerate_quark(), state->error,
			    "libsamplerate has failed: %s",
			    src_strerror(state->error));
		return NULL;
	}

	data->input_frames = src_size / sizeof(*src_buffer) / channels;
	data_in_size = data->input_frames * sizeof(float) * channels;
	data->data_in = pcm_buffer_get(&state->in, data_in_size);

	data->output_frames = (src_size * dest_rate + src_rate - 1) / src_rate;
	data_out_size = data->output_frames * sizeof(float) * channels;
	data->data_out = pcm_buffer_get(&state->out, data_out_size);

	src_int_to_float_array(src_buffer, data->data_in,
			       data->input_frames * channels);

	error = src_process(state->state, data);
	if (error) {
		g_set_error(error_r, libsamplerate_quark(), error,
			    "libsamplerate has failed: %s",
			    src_strerror(error));
		state->error = error;
		return NULL;
	}

	*dest_size_r = data->output_frames_gen *
		sizeof(*dest_buffer) * channels;
	dest_buffer = pcm_buffer_get(&state->buffer, *dest_size_r);
	src_float_to_int_array(data->data_out, dest_buffer,
			       data->output_frames_gen * channels);

	return dest_buffer;
}
