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
#include "pcm_format.h"
#include "pcm_dither.h"
#include "pcm_buffer.h"
#include "pcm_pack.h"

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
pcm_convert_24_to_16(struct pcm_dither *dither,
		     int16_t *out, const int32_t *in,
		     unsigned num_samples)
{
	pcm_dither_24_to_16(dither, out, in, num_samples);
}

static void
pcm_convert_32_to_16(struct pcm_dither *dither,
		     int16_t *out, const int32_t *in,
		     unsigned num_samples)
{
	pcm_dither_32_to_16(dither, out, in, num_samples);
}

static int32_t *
pcm_convert_24_to_24p32(struct pcm_buffer *buffer, const uint8_t *src,
			unsigned num_samples)
{
	int32_t *dest = pcm_buffer_get(buffer, num_samples * 4);
	pcm_unpack_24(dest, src, num_samples, false);
	return dest;
}

const int16_t *
pcm_convert_to_16(struct pcm_buffer *buffer, struct pcm_dither *dither,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	unsigned num_samples;
	int16_t *dest;
	int32_t *dest32;

	switch (src_format) {
	case SAMPLE_FORMAT_UNDEFINED:
		break;

	case SAMPLE_FORMAT_S8:
		num_samples = src_size;
		*dest_size_r = src_size * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_8_to_16(dest,
				    (const int8_t *)src,
				    num_samples);
		return dest;

	case SAMPLE_FORMAT_S16:
		*dest_size_r = src_size;
		return src;

	case SAMPLE_FORMAT_S24:
		/* convert to S24_P32 first */
		num_samples = src_size / 3;

		dest32 = pcm_convert_24_to_24p32(buffer, src, num_samples);
		dest = (int16_t *)dest32;

		/* convert to 16 bit in-place */
		*dest_size_r = num_samples * sizeof(*dest);
		pcm_convert_24_to_16(dither, dest, dest32,
				     num_samples);
		return dest;

	case SAMPLE_FORMAT_S24_P32:
		num_samples = src_size / 4;
		*dest_size_r = num_samples * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_24_to_16(dither, dest,
				     (const int32_t *)src,
				     num_samples);
		return dest;

	case SAMPLE_FORMAT_S32:
		num_samples = src_size / 4;
		*dest_size_r = num_samples * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_32_to_16(dither, dest,
				     (const int32_t *)src,
				     num_samples);
		return dest;
	}

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

static void
pcm_convert_32_to_24(int32_t *out, const int16_t *in,
		     unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ >> 8;
		--num_samples;
	}
}

const int32_t *
pcm_convert_to_24(struct pcm_buffer *buffer,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	unsigned num_samples;
	int32_t *dest;

	switch (src_format) {
	case SAMPLE_FORMAT_UNDEFINED:
		break;

	case SAMPLE_FORMAT_S8:
		num_samples = src_size;
		*dest_size_r = src_size * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_8_to_24(dest, (const int8_t *)src,
				    num_samples);
		return dest;

	case SAMPLE_FORMAT_S16:
		num_samples = src_size / 2;
		*dest_size_r = num_samples * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_16_to_24(dest, (const int16_t *)src,
				     num_samples);
		return dest;

	case SAMPLE_FORMAT_S24:
		num_samples = src_size / 3;
		*dest_size_r = num_samples * sizeof(*dest);

		return pcm_convert_24_to_24p32(buffer, src, num_samples);

	case SAMPLE_FORMAT_S24_P32:
		*dest_size_r = src_size;
		return src;

	case SAMPLE_FORMAT_S32:
		num_samples = src_size / 4;
		*dest_size_r = num_samples * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_32_to_24(dest, (const int16_t *)src,
				     num_samples);
		return dest;
	}

	return NULL;
}

static void
pcm_convert_8_to_32(int32_t *out, const int8_t *in,
		    unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ << 24;
		--num_samples;
	}
}

static void
pcm_convert_16_to_32(int32_t *out, const int16_t *in,
		     unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ << 16;
		--num_samples;
	}
}

static void
pcm_convert_24_to_32(int32_t *out, const int32_t *in,
		     unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++ << 8;
		--num_samples;
	}
}

const int32_t *
pcm_convert_to_32(struct pcm_buffer *buffer,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	unsigned num_samples;
	int32_t *dest;

	switch (src_format) {
	case SAMPLE_FORMAT_UNDEFINED:
		break;

	case SAMPLE_FORMAT_S8:
		num_samples = src_size;
		*dest_size_r = src_size * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_8_to_32(dest, (const int8_t *)src,
				    num_samples);
		return dest;

	case SAMPLE_FORMAT_S16:
		num_samples = src_size / 2;
		*dest_size_r = num_samples * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_16_to_32(dest, (const int16_t *)src,
				     num_samples);
		return dest;

	case SAMPLE_FORMAT_S24:
		/* convert to S24_P32 first */
		num_samples = src_size / 3;

		dest = pcm_convert_24_to_24p32(buffer, src, num_samples);

		/* convert to 32 bit in-place */
		*dest_size_r = num_samples * sizeof(*dest);
		pcm_convert_24_to_32(dest, dest, num_samples);
		return dest;

	case SAMPLE_FORMAT_S24_P32:
		num_samples = src_size / 4;
		*dest_size_r = num_samples * sizeof(*dest);
		dest = pcm_buffer_get(buffer, *dest_size_r);

		pcm_convert_24_to_32(dest, (const int32_t *)src,
				     num_samples);
		return dest;

	case SAMPLE_FORMAT_S32:
		*dest_size_r = src_size;
		return src;
	}

	return NULL;
}
