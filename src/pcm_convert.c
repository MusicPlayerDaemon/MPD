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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pcm_convert.h"
#include "pcm_channels.h"
#include "pcm_format.h"
#include "audio_format.h"

#include <assert.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm"

void pcm_convert_init(struct pcm_convert_state *state)
{
	memset(state, 0, sizeof(*state));

	pcm_resample_init(&state->resample);
	pcm_dither_24_init(&state->dither);

	pcm_buffer_init(&state->format_buffer);
	pcm_buffer_init(&state->channels_buffer);
}

void pcm_convert_deinit(struct pcm_convert_state *state)
{
	pcm_resample_deinit(&state->resample);

	pcm_buffer_deinit(&state->format_buffer);
	pcm_buffer_deinit(&state->channels_buffer);
}

static size_t
pcm_convert_16(const struct audio_format *src_format,
	       const void *src_buffer, size_t src_size,
	       const struct audio_format *dest_format,
	       int16_t *dest_buffer,
	       struct pcm_convert_state *state)
{
	const int16_t *buf;
	size_t len;
	size_t dest_size = pcm_convert_size(src_format, src_size, dest_format);

	assert(dest_format->bits == 16);

	buf = pcm_convert_to_16(&state->format_buffer, &state->dither,
				src_format->bits, src_buffer, src_size,
				&len);
	if (!buf)
		g_error("pcm_convert_to_16() failed");

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_16(&state->channels_buffer,
					      dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (!buf)
			g_error("pcm_convert_channels_16() failed");
	}

	if (src_format->sample_rate != dest_format->sample_rate)
		buf = pcm_resample_16(&state->resample,
				      dest_format->channels,
				      src_format->sample_rate, buf, len,
				      dest_format->sample_rate,
				      &len);

	assert(dest_size >= len);
	memcpy(dest_buffer, buf, len);

	return len;
}

static size_t
pcm_convert_24(const struct audio_format *src_format,
	       const void *src_buffer, size_t src_size,
	       const struct audio_format *dest_format,
	       int32_t *dest_buffer,
	       struct pcm_convert_state *state)
{
	const int32_t *buf;
	size_t len;
	size_t dest_size = pcm_convert_size(src_format, src_size, dest_format);

	assert(dest_format->bits == 24);

	buf = pcm_convert_to_24(&state->format_buffer, src_format->bits,
				src_buffer, src_size, &len);
	if (!buf)
		g_error("pcm_convert_to_24() failed");

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_24(&state->channels_buffer,
					      dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (!buf)
			g_error("pcm_convert_channels_24() failed");
	}

	if (src_format->sample_rate != dest_format->sample_rate)
		buf = pcm_resample_24(&state->resample,
				      dest_format->channels,
				      src_format->sample_rate, buf, len,
				      dest_format->sample_rate,
				      &len);

	assert(dest_size >= len);
	memcpy(dest_buffer, buf, len);

	return len;
}

size_t pcm_convert(const struct audio_format *inFormat,
		   const void *src, size_t src_size,
		   const struct audio_format *outFormat,
		   void *dest,
		   struct pcm_convert_state *convState)
{
	switch (outFormat->bits) {
	case 16:
		return pcm_convert_16(inFormat, src, src_size,
				      outFormat, (int16_t*)dest,
				      convState);
	case 24:
		return pcm_convert_24(inFormat, src, src_size,
				      outFormat, (int32_t*)dest,
				      convState);

	default:
		g_error("cannot convert to %u bit\n", outFormat->bits);
	}
}

size_t pcm_convert_size(const struct audio_format *inFormat, size_t src_size,
			const struct audio_format *outFormat)
{
	const double ratio = (double)outFormat->sample_rate /
	                     (double)inFormat->sample_rate;
	size_t dest_size = src_size;

	/* no partial frames allowed */
	assert((src_size % audio_format_frame_size(inFormat)) == 0);

	dest_size /= audio_format_frame_size(inFormat);
	dest_size = ceil((double)dest_size * ratio);
	dest_size *= audio_format_frame_size(outFormat);

	return dest_size;
}
