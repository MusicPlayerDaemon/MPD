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

#include "pcm_format.h"
#include "pcm_dither.h"

#include <glib.h>

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

const int16_t *
pcm_convert_to_16(struct pcm_dither_24 *dither,
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

		pcm_convert_24_to_16(dither,
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

const int32_t *
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
