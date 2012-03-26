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
#include "pcm_utils.h"

static void
pcm_convert_8_to_16(int16_t *out, const int8_t *in, const int8_t *in_end)
{
	while (in < in_end) {
		*out++ = *in++ << 8;
	}
}

static void
pcm_convert_24_to_16(struct pcm_dither *dither,
		     int16_t *out, const int32_t *in, const int32_t *in_end)
{
	pcm_dither_24_to_16(dither, out, in, in_end);
}

static void
pcm_convert_32_to_16(struct pcm_dither *dither,
		     int16_t *out, const int32_t *in, const int32_t *in_end)
{
	pcm_dither_32_to_16(dither, out, in, in_end);
}

static void
pcm_convert_float_to_16(int16_t *out, const float *in, const float *in_end)
{
	const unsigned OUT_BITS = 16;
	const float factor = 1 << (OUT_BITS - 1);

	while (in < in_end) {
		int sample = *in++ * factor;
		*out++ = pcm_clamp_16(sample);
	}
}

static int16_t *
pcm_allocate_8_to_16(struct pcm_buffer *buffer,
		     const int8_t *src, size_t src_size, size_t *dest_size_r)
{
	int16_t *dest;
	*dest_size_r = src_size / sizeof(*src) * sizeof(*dest);
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_8_to_16(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int16_t *
pcm_allocate_24p32_to_16(struct pcm_buffer *buffer, struct pcm_dither *dither,
			 const int32_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	int16_t *dest;
	*dest_size_r = src_size / 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_24_to_16(dither, dest, src,
			     pcm_end_pointer(src, src_size));
	return dest;
}

static int16_t *
pcm_allocate_32_to_16(struct pcm_buffer *buffer, struct pcm_dither *dither,
		      const int32_t *src, size_t src_size,
		      size_t *dest_size_r)
{
	int16_t *dest;
	*dest_size_r = src_size / 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_32_to_16(dither, dest, src,
			     pcm_end_pointer(src, src_size));
	return dest;
}

static int16_t *
pcm_allocate_float_to_16(struct pcm_buffer *buffer,
			 const float *src, size_t src_size,
			 size_t *dest_size_r)
{
	int16_t *dest;
	*dest_size_r = src_size / 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_float_to_16(dest, src,
				pcm_end_pointer(src, src_size));
	return dest;
}

const int16_t *
pcm_convert_to_16(struct pcm_buffer *buffer, struct pcm_dither *dither,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	assert(src_size % sample_format_size(src_format) == 0);

	switch (src_format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_DSD:
		break;

	case SAMPLE_FORMAT_S8:
		return pcm_allocate_8_to_16(buffer,
					    src, src_size, dest_size_r);

	case SAMPLE_FORMAT_S16:
		*dest_size_r = src_size;
		return src;

	case SAMPLE_FORMAT_S24_P32:
		return pcm_allocate_24p32_to_16(buffer, dither, src, src_size,
						dest_size_r);

	case SAMPLE_FORMAT_S32:
		return pcm_allocate_32_to_16(buffer, dither, src, src_size,
					     dest_size_r);

	case SAMPLE_FORMAT_FLOAT:
		return pcm_allocate_float_to_16(buffer, src, src_size,
						dest_size_r);
	}

	return NULL;
}

static void
pcm_convert_8_to_24(int32_t *out, const int8_t *in, const int8_t *in_end)
{
	while (in < in_end)
		*out++ = *in++ << 16;
}

static void
pcm_convert_16_to_24(int32_t *out, const int16_t *in, const int16_t *in_end)
{
	while (in < in_end)
		*out++ = *in++ << 8;
}

static void
pcm_convert_32_to_24(int32_t *restrict out,
		     const int32_t *restrict in,
		     const int32_t *restrict in_end)
{
	while (in < in_end)
		*out++ = *in++ >> 8;
}

static void
pcm_convert_float_to_24(int32_t *out, const float *in, const float *in_end)
{
	const unsigned OUT_BITS = 24;
	const float factor = 1 << (OUT_BITS - 1);

	while (in < in_end) {
		int sample = *in++ * factor;
		*out++ = pcm_clamp_24(sample);
	}
}

static int32_t *
pcm_allocate_8_to_24(struct pcm_buffer *buffer,
		     const int8_t *src, size_t src_size, size_t *dest_size_r)
{
	int32_t *dest;
	*dest_size_r = src_size / sizeof(*src) * sizeof(*dest);
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_8_to_24(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_16_to_24(struct pcm_buffer *buffer,
		      const int16_t *src, size_t src_size, size_t *dest_size_r)
{
	int32_t *dest;
	*dest_size_r = src_size * 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_16_to_24(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_32_to_24(struct pcm_buffer *buffer,
		      const int32_t *src, size_t src_size, size_t *dest_size_r)
{
	*dest_size_r = src_size;
	int32_t *dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_32_to_24(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_float_to_24(struct pcm_buffer *buffer,
			 const float *src, size_t src_size,
			 size_t *dest_size_r)
{
	*dest_size_r = src_size;
	int32_t *dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_float_to_24(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

const int32_t *
pcm_convert_to_24(struct pcm_buffer *buffer,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	assert(src_size % sample_format_size(src_format) == 0);

	switch (src_format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_DSD:
		break;

	case SAMPLE_FORMAT_S8:
		return pcm_allocate_8_to_24(buffer,
					    src, src_size, dest_size_r);

	case SAMPLE_FORMAT_S16:
		return pcm_allocate_16_to_24(buffer,
					    src, src_size, dest_size_r);

	case SAMPLE_FORMAT_S24_P32:
		*dest_size_r = src_size;
		return src;

	case SAMPLE_FORMAT_S32:
		return pcm_allocate_32_to_24(buffer, src, src_size,
					     dest_size_r);

	case SAMPLE_FORMAT_FLOAT:
		return pcm_allocate_float_to_24(buffer, src, src_size,
						dest_size_r);
	}

	return NULL;
}

static void
pcm_convert_8_to_32(int32_t *out, const int8_t *in, const int8_t *in_end)
{
	while (in < in_end)
		*out++ = *in++ << 24;
}

static void
pcm_convert_16_to_32(int32_t *out, const int16_t *in, const int16_t *in_end)
{
	while (in < in_end)
		*out++ = *in++ << 16;
}

static void
pcm_convert_24_to_32(int32_t *restrict out,
		     const int32_t *restrict in,
		     const int32_t *restrict in_end)
{
	while (in < in_end)
		*out++ = *in++ << 8;
}

static int32_t *
pcm_allocate_8_to_32(struct pcm_buffer *buffer,
		     const int8_t *src, size_t src_size, size_t *dest_size_r)
{
	int32_t *dest;
	*dest_size_r = src_size / sizeof(*src) * sizeof(*dest);
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_8_to_32(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_16_to_32(struct pcm_buffer *buffer,
		      const int16_t *src, size_t src_size, size_t *dest_size_r)
{
	int32_t *dest;
	*dest_size_r = src_size * 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_16_to_32(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_24p32_to_32(struct pcm_buffer *buffer,
			 const int32_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	*dest_size_r = src_size;
	int32_t *dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_24_to_32(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static int32_t *
pcm_allocate_float_to_32(struct pcm_buffer *buffer,
			 const float *src, size_t src_size,
			 size_t *dest_size_r)
{
	/* convert to S24_P32 first */
	int32_t *dest = pcm_allocate_float_to_24(buffer, src, src_size,
						 dest_size_r);

	/* convert to 32 bit in-place */
	pcm_convert_24_to_32(dest, dest, pcm_end_pointer(dest, *dest_size_r));
	return dest;
}

const int32_t *
pcm_convert_to_32(struct pcm_buffer *buffer,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r)
{
	assert(src_size % sample_format_size(src_format) == 0);

	switch (src_format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_DSD:
		break;

	case SAMPLE_FORMAT_S8:
		return pcm_allocate_8_to_32(buffer, src, src_size,
					    dest_size_r);

	case SAMPLE_FORMAT_S16:
		return pcm_allocate_16_to_32(buffer, src, src_size,
					     dest_size_r);

	case SAMPLE_FORMAT_S24_P32:
		return pcm_allocate_24p32_to_32(buffer, src, src_size,
						dest_size_r);

	case SAMPLE_FORMAT_S32:
		*dest_size_r = src_size;
		return src;

	case SAMPLE_FORMAT_FLOAT:
		return pcm_allocate_float_to_32(buffer, src, src_size,
						dest_size_r);
	}

	return NULL;
}

static void
pcm_convert_8_to_float(float *out, const int8_t *in, const int8_t *in_end)
{
	enum { in_bits = sizeof(*in) * 8 };
	static const float factor = 2.0f / (1 << in_bits);
	while (in < in_end)
		*out++ = (float)*in++ * factor;
}

static void
pcm_convert_16_to_float(float *out, const int16_t *in, const int16_t *in_end)
{
	enum { in_bits = sizeof(*in) * 8 };
	static const float factor = 2.0f / (1 << in_bits);
	while (in < in_end)
		*out++ = (float)*in++ * factor;
}

static void
pcm_convert_24_to_float(float *out, const int32_t *in, const int32_t *in_end)
{
	enum { in_bits = 24 };
	static const float factor = 2.0f / (1 << in_bits);
	while (in < in_end)
		*out++ = (float)*in++ * factor;
}

static void
pcm_convert_32_to_float(float *out, const int32_t *in, const int32_t *in_end)
{
	enum { in_bits = sizeof(*in) * 8 };
	static const float factor = 0.5f / (1 << (in_bits - 2));
	while (in < in_end)
		*out++ = (float)*in++ * factor;
}

static float *
pcm_allocate_8_to_float(struct pcm_buffer *buffer,
			const int8_t *src, size_t src_size,
			size_t *dest_size_r)
{
	float *dest;
	*dest_size_r = src_size / sizeof(*src) * sizeof(*dest);
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_8_to_float(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static float *
pcm_allocate_16_to_float(struct pcm_buffer *buffer,
			 const int16_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	float *dest;
	*dest_size_r = src_size * 2;
	assert(*dest_size_r == src_size / sizeof(*src) * sizeof(*dest));
	dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_16_to_float(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static float *
pcm_allocate_24p32_to_float(struct pcm_buffer *buffer,
			 const int32_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	*dest_size_r = src_size;
	float *dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_24_to_float(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

static float *
pcm_allocate_32_to_float(struct pcm_buffer *buffer,
			 const int32_t *src, size_t src_size,
			 size_t *dest_size_r)
{
	*dest_size_r = src_size;
	float *dest = pcm_buffer_get(buffer, *dest_size_r);
	pcm_convert_32_to_float(dest, src, pcm_end_pointer(src, src_size));
	return dest;
}

const float *
pcm_convert_to_float(struct pcm_buffer *buffer,
		     enum sample_format src_format, const void *src,
		     size_t src_size, size_t *dest_size_r)
{
	switch (src_format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_DSD:
		break;

	case SAMPLE_FORMAT_S8:
		return pcm_allocate_8_to_float(buffer,
					       src, src_size, dest_size_r);

	case SAMPLE_FORMAT_S16:
		return pcm_allocate_16_to_float(buffer,
						src, src_size, dest_size_r);

	case SAMPLE_FORMAT_S24_P32:
		return pcm_allocate_24p32_to_float(buffer,
						   src, src_size, dest_size_r);

	case SAMPLE_FORMAT_S32:
		return pcm_allocate_32_to_float(buffer,
						src, src_size, dest_size_r);

	case SAMPLE_FORMAT_FLOAT:
		*dest_size_r = src_size;
		return src;
	}

	return NULL;
}
