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
#include "pcm_prng.h"
#include "utils.h"
#include "conf.h"
#include "audio_format.h"

#include <assert.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm"

static inline int
pcm_dither(void)
{
	static unsigned long state;
	uint32_t r;

	r = state = prng(state);

	return (r & 511) - ((r >> 9) & 511);
}

/**
 * Check if the value is within the range of the provided bit size,
 * and caps it if necessary.
 */
static int32_t
pcm_range(int32_t sample, unsigned bits)
{
	if (G_UNLIKELY(sample < (-1 << (bits - 1))))
		return -1 << (bits - 1);
	if (G_UNLIKELY(sample >= (1 << (bits - 1))))
		return (1 << (bits - 1)) - 1;
	return sample;
}

static void
pcm_volume_change_8(int8_t *buffer, unsigned num_samples, int volume)
{
	while (num_samples > 0) {
		int32_t sample = *buffer;

		sample = (sample * volume + pcm_dither() + PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer++ = pcm_range(sample, 8);
		--num_samples;
	}
}

static void
pcm_volume_change_16(int16_t *buffer, unsigned num_samples, int volume)
{
	while (num_samples > 0) {
		int32_t sample = *buffer;

		sample = (sample * volume + pcm_dither() + PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer++ = pcm_range(sample, 16);
		--num_samples;
	}
}

static void
pcm_volume_change_24(int32_t *buffer, unsigned num_samples, int volume)
{
	while (num_samples > 0) {
		int64_t sample = *buffer;

		sample = (sample * volume + pcm_dither() + PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer++ = pcm_range(sample, 24);
		--num_samples;
	}
}

void pcm_volume(char *buffer, int bufferSize,
		const struct audio_format *format,
		int volume)
{
	if (volume == PCM_VOLUME_1)
		return;

	if (volume <= 0) {
		memset(buffer, 0, bufferSize);
		return;
	}

	switch (format->bits) {
	case 8:
		pcm_volume_change_8((int8_t *)buffer, bufferSize, volume);
		break;

	case 16:
		pcm_volume_change_16((int16_t *)buffer, bufferSize / 2,
				     volume);
		break;

	case 24:
		pcm_volume_change_24((int32_t*)buffer, bufferSize / 4,
				     volume);
		break;

	default:
		g_error("%u bits not supported by pcm_volume!\n",
			format->bits);
	}
}

static void
pcm_add_8(int8_t *buffer1, const int8_t *buffer2,
	  unsigned num_samples, int volume1, int volume2)
{
	while (num_samples > 0) {
		int32_t sample1 = *buffer1;
		int32_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_dither() + PCM_VOLUME_1 / 2) / PCM_VOLUME_1;

		*buffer1++ = pcm_range(sample1, 8);
		--num_samples;
	}
}

static void
pcm_add_16(int16_t *buffer1, const int16_t *buffer2,
	   unsigned num_samples, int volume1, int volume2)
{
	while (num_samples > 0) {
		int32_t sample1 = *buffer1;
		int32_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_dither() + PCM_VOLUME_1 / 2) / PCM_VOLUME_1;

		*buffer1++ = pcm_range(sample1, 16);
		--num_samples;
	}
}

static void
pcm_add_24(int32_t *buffer1, const int32_t *buffer2,
	   unsigned num_samples, unsigned volume1, unsigned volume2)
{
	while (num_samples > 0) {
		int64_t sample1 = *buffer1;
		int64_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_dither() + PCM_VOLUME_1 / 2) / PCM_VOLUME_1;

		*buffer1++ = pcm_range(sample1, 24);
		--num_samples;
	}
}

static void pcm_add(char *buffer1, const char *buffer2, size_t size,
                    int vol1, int vol2,
                    const struct audio_format *format)
{
	switch (format->bits) {
	case 8:
		pcm_add_8((int8_t *)buffer1, (const int8_t *)buffer2,
			  size, vol1, vol2);
		break;

	case 16:
		pcm_add_16((int16_t *)buffer1, (const int16_t *)buffer2,
			   size / 2, vol1, vol2);
		break;

	case 24:
		pcm_add_24((int32_t*)buffer1,
			   (const int32_t*)buffer2,
			   size / 4, vol1, vol2);
		break;

	default:
		g_error("%u bits not supported by pcm_add!\n", format->bits);
	}
}

void pcm_mix(char *buffer1, const char *buffer2, size_t size,
             const struct audio_format *format, float portion1)
{
	int vol1;
	float s = sin(M_PI_2 * portion1);
	s *= s;

	vol1 = s * PCM_VOLUME_1 + 0.5;
	vol1 = vol1 > PCM_VOLUME_1 ? PCM_VOLUME_1 : (vol1 < 0 ? 0 : vol1);

	pcm_add(buffer1, buffer2, size, vol1, PCM_VOLUME_1 - vol1, format);
}

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
			buf = xrealloc(buf, len);
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
			buf = xrealloc(buf, len);
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
			buf = xrealloc(buf, len);
		}

		pcm_convert_8_to_24(buf, (const int8_t *)src,
				    num_samples);
		return buf;

	case 16:
		num_samples = src_size / 2;
		*dest_size_r = num_samples * 4;
		if (*dest_size_r > len) {
			len = *dest_size_r;
			buf = xrealloc(buf, len);
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
		exit(EXIT_FAILURE);

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_16(dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (!buf)
			exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_24(dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (!buf)
			exit(EXIT_FAILURE);
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
