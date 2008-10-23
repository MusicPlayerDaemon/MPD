/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pcm_resample.h"
#include "conf.h"
#include "log.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static int pcm_resample_get_converter(void)
{
	const char *conf = getConfigParamValue(CONF_SAMPLERATE_CONVERTER);
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
		if (strncasecmp(test2, conf, len) == 0)
			goto out;
	}

	ERROR("unknown samplerate converter \"%s\"\n", conf);
out:
	DEBUG("selecting samplerate converter \"%s\"\n",
	      src_get_name(convalgo));

	return convalgo;
}

size_t
pcm_resample_16(uint8_t channels,
		unsigned src_rate,
		const int16_t *src_buffer, size_t src_size,
		unsigned dest_rate,
		int16_t *dest_buffer, size_t dest_size,
		struct pcm_resample_state *state)
{
	static int convalgo = -1;
	SRC_DATA *data = &state->data;
	size_t data_in_size;
	size_t data_out_size;
	int error;

	if (convalgo < 0)
		convalgo = pcm_resample_get_converter();

	/* (re)set the state/ratio if the in or out format changed */
	if (channels != state->prev.channels ||
	    src_rate != state->prev.src_rate ||
	    dest_rate != state->prev.dest_rate) {
		state->error = false;
		state->prev.channels = channels;
		state->prev.src_rate = src_rate;
		state->prev.dest_rate = dest_rate;

		if (state->state)
			state->state = src_delete(state->state);

		state->state = src_new(convalgo, channels, &error);
		if (!state->state) {
			ERROR("cannot create new libsamplerate state: %s\n",
			      src_strerror(error));
			state->error = true;
			return 0;
		}

		data->src_ratio = (double)dest_rate / (double)src_rate;
		DEBUG("setting samplerate conversion ratio to %.2lf\n",
		      data->src_ratio);
		src_set_ratio(state->state, data->src_ratio);
	}

	/* there was an error previously, and nothing has changed */
	if (state->error)
		return 0;

	data->input_frames = src_size / sizeof(*src_buffer) / channels;
	data_in_size = data->input_frames * sizeof(float) * channels;
	if (data_in_size > state->data_in_size) {
		state->data_in_size = data_in_size;
		data->data_in = xrealloc(data->data_in, data_in_size);
	}

	data->output_frames = dest_size / sizeof(*dest_buffer) / channels;
	data_out_size = data->output_frames * sizeof(float) * channels;
	if (data_out_size > state->data_out_size) {
		state->data_out_size = data_out_size;
		data->data_out = xrealloc(data->data_out, data_out_size);
	}

	src_short_to_float_array(src_buffer, data->data_in,
				 data->input_frames * channels);

	error = src_process(state->state, data);
	if (error) {
		ERROR("error processing samples with libsamplerate: %s\n",
		      src_strerror(error));
		state->error = true;
		return 0;
	}

	src_float_to_short_array(data->data_out, dest_buffer,
				 data->output_frames_gen * channels);

	return data->output_frames_gen * sizeof(*dest_buffer) * channels;
}
