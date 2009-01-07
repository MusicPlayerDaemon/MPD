/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#include "pcm_utils.h"
#include "pcm_channels.h"
#include "conf.h"
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
}

static void
pcm_convert_8_to_16(int16_t *out, const int8_t *in,
		    unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ << 8;
		--num_samples;
	}
}

static void
pcm_convert_24_to_16(struct pcm_dither_24 *dither,
		     int16_t *out, const int32_t *in,
		     unsigned num_samples)
{
	pcm_dither_24_to_16(dither, out, in, num_samples);
}

static const int16_t *
pcm_convert_to_16(struct pcm_convert_state *convert,
		  uint8_t bits, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	static int16_t *buf;
	static size_t len;
	unsigned num_samples;

	switch (bits) {
	case 8:
		num_samples = src_size;
		*dest_size_r = src_size << 1;
		if (*dest_size_r > len) {
			len = *dest_size_r;
			buf = g_realloc(buf, len);
		}

		pcm_convert_8_to_16((int16_t *)buf,
				    (const int8_t *)src,
				    num_samples);
		return buf;

	case 16:
		*dest_size_r = src_size;
		return src;

	case 24:
		num_samples = src_size / 4;
		*dest_size_r = num_samples * 2;
		if (*dest_size_r > len) {
			len = *dest_size_r;
			buf = g_realloc(buf, len);
		}

		pcm_convert_24_to_16(&convert->dither,
				     (int16_t *)buf,
				     (const int32_t *)src,
				     num_samples);
		return buf;
	}

	g_warning("only 8 or 16 bits are supported for conversion!\n");
	return NULL;
}

static void
pcm_convert_8_to_24(int32_t *out, const int8_t *in,
		    unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ << 16;
		--num_samples;
	}
}

static void
pcm_convert_16_to_24(int32_t *out, const int16_t *in,
		     unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ << 8;
		--num_samples;
	}
}

static const int32_t *
pcm_convert_to_24(uint8_t bits, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	static int32_t *buf;
	static size_t len;
	unsigned num_samples;

	switch (bits) {
	case 8:
		num_samples = src_size;
		*dest_size_r = src_size * 4;
		if (*dest_size_r > len) {
			len = *dest_size_r;
			buf = g_realloc(buf, len);
		}

		pcm_convert_8_to_24(buf, (const int8_t *)src,
				    num_samples);
		return buf;

	case 16:
		num_samples = src_size / 2;
		*dest_size_r = num_samples * 4;
		if (*dest_size_r > len) {
			len = *dest_size_r;
			buf = g_realloc(buf, len);
		}

		pcm_convert_16_to_24(buf, (const int16_t *)src,
				     num_samples);
		return buf;

	case 24:
		*dest_size_r = src_size;
		return src;
	}

	g_warning("only 8 or 24 bits are supported for conversion!\n");
	return NULL;
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

	buf = pcm_convert_to_16(state, src_format->bits,
				src_buffer, src_size, &len);
	if (!buf)
		g_error("pcm_convert_to_16() failed");

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_16(dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (!buf)
			g_error("pcm_convert_channels_16() failed");
	}

	if (src_format->sample_rate == dest_format->sample_rate) {
		assert(dest_size >= len);
		memcpy(dest_buffer, buf, len);
	} else {
		len = pcm_resample_16(dest_format->channels,
				      src_format->sample_rate, buf, len,
				      dest_format->sample_rate,
				      dest_buffer, dest_size,
				      &state->resample);
	}

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

	buf = pcm_convert_to_24(src_format->bits,
				src_buffer, src_size, &len);
	if (!buf)
		g_error("pcm_convert_to_24() failed");

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_24(dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (!buf)
			g_error("pcm_convert_channels_24() failed");
	}

	if (src_format->sample_rate == dest_format->sample_rate) {
		assert(dest_size >= len);
		memcpy(dest_buffer, buf, len);
	} else {
		len = pcm_resample_24(dest_format->channels,
				      src_format->sample_rate, buf, len,
				      dest_format->sample_rate,
				      (int32_t*)dest_buffer, dest_size,
				      &state->resample);
	}

	return len;
}

/* outFormat bits must be 16 and channels must be 1 or 2! */
size_t pcm_convert(const struct audio_format *inFormat,
		   const char *src, size_t src_size,
		   const struct audio_format *outFormat,
		   char *outBuffer,
		   struct pcm_convert_state *convState)
{
	switch (outFormat->bits) {
	case 16:
		return pcm_convert_16(inFormat, src, src_size,
				      outFormat, (int16_t*)outBuffer,
				      convState);
	case 24:
		return pcm_convert_24(inFormat, src, src_size,
				      outFormat, (int32_t*)outBuffer,
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
