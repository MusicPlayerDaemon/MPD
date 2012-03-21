/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "util/byte_reverse.h"

#include <assert.h>

const int16_t *pcm_byteswap_16(struct pcm_buffer *buffer,
			       const int16_t *src, size_t len)
{
	int16_t *buf = pcm_buffer_get(buffer, len);

	assert(buf != NULL);

	const uint8_t *src8 = (const uint8_t *)src;
	const void *src_end = src8 + len;

	reverse_bytes_16((uint16_t *)buf, (const uint16_t *)src, src_end);
	return buf;
}

const int32_t *pcm_byteswap_32(struct pcm_buffer *buffer,
			       const int32_t *src, size_t len)
{
	int32_t *buf = pcm_buffer_get(buffer, len);

	assert(buf != NULL);

	const uint8_t *src8 = (const uint8_t *)src;
	const void *src_end = src8 + len;

	reverse_bytes_32((uint32_t *)buf, (const uint32_t *)src, src_end);
	return buf;
}

const void *
pcm_byteswap(struct pcm_buffer *buffer, enum sample_format format,
	     const void *_src, size_t size)
{
	const uint8_t *const src = _src;

	if (size <= 1)
		return src;

	size_t sample_size = sample_format_size(format);
	if (sample_size <= 1)
		return src;

	assert(size % sample_size == 0);

	uint8_t *dest = pcm_buffer_get(buffer, size);
	assert(dest != NULL);

	reverse_bytes(dest, src, src + size, sample_size);
	return dest;
}
