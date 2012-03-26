/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "pcm_convert.h"
#include "pcm_channels.h"
#include "pcm_format.h"
#include "pcm_pack.h"
#include "audio_format.h"
#include "glib_compat.h"

#include <assert.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm"

void pcm_convert_init(struct pcm_convert_state *state)
{
	memset(state, 0, sizeof(*state));

	pcm_dsd_init(&state->dsd);
	pcm_resample_init(&state->resample);
	pcm_dither_24_init(&state->dither);

	pcm_buffer_init(&state->format_buffer);
	pcm_buffer_init(&state->channels_buffer);
}

void pcm_convert_deinit(struct pcm_convert_state *state)
{
	pcm_dsd_deinit(&state->dsd);
	pcm_resample_deinit(&state->resample);

	pcm_buffer_deinit(&state->format_buffer);
	pcm_buffer_deinit(&state->channels_buffer);
}

void
pcm_convert_reset(struct pcm_convert_state *state)
{
	pcm_dsd_reset(&state->dsd);
	pcm_resample_reset(&state->resample);
}

static const void *
pcm_convert_channels(struct pcm_buffer *buffer, enum sample_format format,
		     uint8_t dest_channels,
		     uint8_t src_channels, const void *src,
		     size_t src_size, size_t *dest_size_r,
		     GError **error_r)
{
	const void *dest = NULL;

	switch (format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_S8:
	case SAMPLE_FORMAT_FLOAT:
	case SAMPLE_FORMAT_DSD:
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Channel conversion not implemented for format '%s'",
			    sample_format_to_string(format));
		return NULL;

	case SAMPLE_FORMAT_S16:
		dest = pcm_convert_channels_16(buffer, dest_channels,
					       src_channels, src,
					       src_size, dest_size_r);
		break;

	case SAMPLE_FORMAT_S24_P32:
		dest = pcm_convert_channels_24(buffer, dest_channels,
					       src_channels, src,
					       src_size, dest_size_r);
		break;

	case SAMPLE_FORMAT_S32:
		dest = pcm_convert_channels_32(buffer, dest_channels,
					       src_channels, src,
					       src_size, dest_size_r);
		break;
	}

	if (dest == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %u to %u channels "
			    "is not implemented",
			    src_channels, dest_channels);
		return NULL;
	}

	return dest;
}

static const int16_t *
pcm_convert_16(struct pcm_convert_state *state,
	       const struct audio_format *src_format,
	       const void *src_buffer, size_t src_size,
	       const struct audio_format *dest_format, size_t *dest_size_r,
	       GError **error_r)
{
	const int16_t *buf;
	size_t len;

	assert(dest_format->format == SAMPLE_FORMAT_S16);

	buf = pcm_convert_to_16(&state->format_buffer, &state->dither,
				src_format->format, src_buffer, src_size,
				&len);
	if (buf == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %s to 16 bit is not implemented",
			    sample_format_to_string(src_format->format));
		return NULL;
	}

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_16(&state->channels_buffer,
					      dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (buf == NULL) {
			g_set_error(error_r, pcm_convert_quark(), 0,
				    "Conversion from %u to %u channels "
				    "is not implemented",
				    src_format->channels,
				    dest_format->channels);
			return NULL;
		}
	}

	if (src_format->sample_rate != dest_format->sample_rate) {
		buf = pcm_resample_16(&state->resample,
				      dest_format->channels,
				      src_format->sample_rate, buf, len,
				      dest_format->sample_rate, &len,
				      error_r);
		if (buf == NULL)
			return NULL;
	}

	*dest_size_r = len;
	return buf;
}

static const int32_t *
pcm_convert_24(struct pcm_convert_state *state,
	       const struct audio_format *src_format,
	       const void *src_buffer, size_t src_size,
	       const struct audio_format *dest_format, size_t *dest_size_r,
	       GError **error_r)
{
	const int32_t *buf;
	size_t len;

	assert(dest_format->format == SAMPLE_FORMAT_S24_P32);

	buf = pcm_convert_to_24(&state->format_buffer, src_format->format,
				src_buffer, src_size, &len);
	if (buf == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %s to 24 bit is not implemented",
			    sample_format_to_string(src_format->format));
		return NULL;
	}

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_24(&state->channels_buffer,
					      dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (buf == NULL) {
			g_set_error(error_r, pcm_convert_quark(), 0,
				    "Conversion from %u to %u channels "
				    "is not implemented",
				    src_format->channels,
				    dest_format->channels);
			return NULL;
		}
	}

	if (src_format->sample_rate != dest_format->sample_rate) {
		buf = pcm_resample_24(&state->resample,
				      dest_format->channels,
				      src_format->sample_rate, buf, len,
				      dest_format->sample_rate, &len,
				      error_r);
		if (buf == NULL)
			return NULL;
	}

	*dest_size_r = len;
	return buf;
}

static const int32_t *
pcm_convert_32(struct pcm_convert_state *state,
	       const struct audio_format *src_format,
	       const void *src_buffer, size_t src_size,
	       const struct audio_format *dest_format, size_t *dest_size_r,
	       GError **error_r)
{
	const int32_t *buf;
	size_t len;

	assert(dest_format->format == SAMPLE_FORMAT_S32);

	buf = pcm_convert_to_32(&state->format_buffer, src_format->format,
				src_buffer, src_size, &len);
	if (buf == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %s to 32 bit is not implemented",
			    sample_format_to_string(src_format->format));
		return NULL;
	}

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_32(&state->channels_buffer,
					      dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (buf == NULL) {
			g_set_error(error_r, pcm_convert_quark(), 0,
				    "Conversion from %u to %u channels "
				    "is not implemented",
				    src_format->channels,
				    dest_format->channels);
			return NULL;
		}
	}

	if (src_format->sample_rate != dest_format->sample_rate) {
		buf = pcm_resample_32(&state->resample,
				      dest_format->channels,
				      src_format->sample_rate, buf, len,
				      dest_format->sample_rate, &len,
				      error_r);
		if (buf == NULL)
			return buf;
	}

	*dest_size_r = len;
	return buf;
}

static const float *
pcm_convert_float(struct pcm_convert_state *state,
		  const struct audio_format *src_format,
		  const void *src_buffer, size_t src_size,
		  const struct audio_format *dest_format, size_t *dest_size_r,
		  GError **error_r)
{
	const float *buffer = src_buffer;
	size_t size = src_size;

	assert(dest_format->format == SAMPLE_FORMAT_FLOAT);

	/* convert channels first, hoping the source format is
	   supported (float is not) */

	if (dest_format->channels != src_format->channels) {
		buffer = pcm_convert_channels(&state->channels_buffer,
					      src_format->format,
					      dest_format->channels,
					      src_format->channels,
					      buffer, size, &size, error_r);
		if (buffer == NULL)
			return NULL;
	}

	/* convert to float now */

	buffer = pcm_convert_to_float(&state->format_buffer,
				      src_format->format,
				      buffer, size, &size);
	if (buffer == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %s to float is not implemented",
			    sample_format_to_string(src_format->format));
		return NULL;
	}

	/* resample with float, because this is the best format for
	   libsamplerate */

	if (src_format->sample_rate != dest_format->sample_rate) {
		buffer = pcm_resample_float(&state->resample,
					    dest_format->channels,
					    src_format->sample_rate,
					    buffer, size,
					    dest_format->sample_rate, &size,
					    error_r);
		if (buffer == NULL)
			return NULL;
	}

	*dest_size_r = size;
	return buffer;
}

const void *
pcm_convert(struct pcm_convert_state *state,
	    const struct audio_format *src_format,
	    const void *src, size_t src_size,
	    const struct audio_format *dest_format,
	    size_t *dest_size_r,
	    GError **error_r)
{
	struct audio_format float_format;
	if (src_format->format == SAMPLE_FORMAT_DSD) {
		size_t f_size;
		const float *f = pcm_dsd_to_float(&state->dsd,
						  src_format->channels,
						  false, src, src_size,
						  &f_size);
		if (f == NULL) {
			g_set_error_literal(error_r, pcm_convert_quark(), 0,
					    "DSD to PCM conversion failed");
			return NULL;
		}

		float_format = *src_format;
		float_format.format = SAMPLE_FORMAT_FLOAT;

		src_format = &float_format;
		src = f;
		src_size = f_size;
	}

	switch (dest_format->format) {
	case SAMPLE_FORMAT_S16:
		return pcm_convert_16(state,
				      src_format, src, src_size,
				      dest_format, dest_size_r,
				      error_r);

	case SAMPLE_FORMAT_S24_P32:
		return pcm_convert_24(state,
				      src_format, src, src_size,
				      dest_format, dest_size_r,
				      error_r);

	case SAMPLE_FORMAT_S32:
		return pcm_convert_32(state,
				      src_format, src, src_size,
				      dest_format, dest_size_r,
				      error_r);

	case SAMPLE_FORMAT_FLOAT:
		return pcm_convert_float(state,
					 src_format, src, src_size,
					 dest_format, dest_size_r,
					 error_r);

	default:
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "PCM conversion to %s is not implemented",
			    sample_format_to_string(dest_format->format));
		return NULL;
	}
}
