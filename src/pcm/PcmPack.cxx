/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "PcmPack.hxx"
#include "system/ByteOrder.hxx"

static void
pack_sample(uint8_t *dest, const int32_t *src0)
{
	const uint8_t *src = (const uint8_t *)src0;

	if (IsBigEndian())
		++src;

	*dest++ = *src++;
	*dest++ = *src++;
	*dest++ = *src++;
}

void
pcm_pack_24(uint8_t *dest, const int32_t *src, const int32_t *src_end)
{
	/* duplicate loop to help the compiler's optimizer (constant
	   parameter to the pack_sample() inline function) */

	while (src < src_end) {
		pack_sample(dest, src++);
		dest += 3;
	}
}

static void
unpack_sample(int32_t *dest0, const uint8_t *src)
{
	uint8_t *dest = (uint8_t *)dest0;

	if (IsBigEndian())
		/* extend the sign bit to the most fourth byte */
		*dest++ = *src & 0x80 ? 0xff : 0x00;

	*dest++ = *src++;
	*dest++ = *src++;
	*dest++ = *src;

	if (IsLittleEndian())
		/* extend the sign bit to the most fourth byte */
		*dest++ = *src & 0x80 ? 0xff : 0x00;
}

void
pcm_unpack_24(int32_t *dest, const uint8_t *src, const uint8_t *src_end)
{
	/* duplicate loop to help the compiler's optimizer (constant
	   parameter to the unpack_sample() inline function) */

	while (src < src_end) {
		unpack_sample(dest++, src);
		src += 3;
	}
}
