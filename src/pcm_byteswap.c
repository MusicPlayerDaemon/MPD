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

static inline uint16_t swab16(uint16_t x)
{
	return (x << 8) | (x >> 8);
}

const int16_t *pcm_byteswap_16(struct pcm_buffer *buffer,
			       const int16_t *src, size_t len)
{
	unsigned i;
	int16_t *buf = pcm_buffer_get(buffer, len);

	assert(buf != NULL);

	for (i = 0; i < len / 2; i++)
		buf[i] = swab16(src[i]);

	return buf;
}

static inline uint32_t swab32(uint32_t x)
{
	return (x << 24) | 
		((x & 0xff00) << 8) |
		((x & 0xff0000) >> 8) |
		(x >> 24);
}

const int32_t *pcm_byteswap_32(struct pcm_buffer *buffer,
			       const int32_t *src, size_t len)
{
	unsigned i;
	int32_t *buf = pcm_buffer_get(buffer, len);

	assert(buf != NULL);

	for (i = 0; i < len / 4; i++)
		buf[i] = swab32(src[i]);

	return buf;
}
