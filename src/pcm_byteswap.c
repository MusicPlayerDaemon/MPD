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
#include "pcm_byteswap.h"
#include "pcm_buffer.h"

#include <glib.h>

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm"

const int16_t *pcm_byteswap_16(struct pcm_buffer *buffer,
			       const int16_t *src, size_t len)
{
	int16_t *buf = pcm_buffer_get(buffer, len);

	assert(buf != NULL);

	const int16_t *src_end = src + len / sizeof(*src);
	int16_t *dest = buf;
	while (src < src_end) {
		const int16_t x = *src++;
		*dest++ = GUINT16_SWAP_LE_BE(x);
	}

	return buf;
}

const int32_t *pcm_byteswap_32(struct pcm_buffer *buffer,
			       const int32_t *src, size_t len)
{
	int32_t *buf = pcm_buffer_get(buffer, len);

	assert(buf != NULL);

	const int32_t *src_end = src + len / sizeof(*src);
	int32_t *dest = buf;
	while (src < src_end) {
		const int32_t x = *src++;
		*dest++ = GUINT32_SWAP_LE_BE(x);
	}

	return buf;
}

const void *
pcm_byteswap(struct pcm_buffer *buffer, enum sample_format format,
	     const void *src, size_t size)
{
	switch (format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_S24:
	case SAMPLE_FORMAT_FLOAT:
		/* not implemented */
		return NULL;

	case SAMPLE_FORMAT_S8:
	case SAMPLE_FORMAT_DSD:
	case SAMPLE_FORMAT_DSD_LSBFIRST:
		return src;

	case SAMPLE_FORMAT_S16:
		return pcm_byteswap_16(buffer, src, size);

	case SAMPLE_FORMAT_S24_P32:
	case SAMPLE_FORMAT_S32:
		return pcm_byteswap_32(buffer, src, size);
	}

	/* unreachable */
	assert(false);
	return NULL;
}
