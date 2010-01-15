/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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

/** \file
 *
 * Library for working with packed 24 bit samples.
 */

#ifndef PCM_PACK_H
#define PCM_PACK_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Converts padded 24 bit samples (4 bytes per sample) to packed 24
 * bit samples (3 bytes per sample).
 *
 * This function can be used to convert a buffer in-place.
 *
 * @param dest the destination buffer (array of triples)
 * @param src the source buffer
 * @param num_samples the number of samples to convert
 * @param reverse_endian is src and dest in non-host byte order?
 */
void
pcm_pack_24(uint8_t *dest, const int32_t *src, unsigned num_samples,
	    bool reverse_endian);

/**
 * Converts packed 24 bit samples (3 bytes per sample) to padded 24
 * bit samples (4 bytes per sample).
 *
 * @param dest the destination buffer
 * @param src the source buffer (array of triples)
 * @param num_samples the number of samples to convert
 * @param reverse_endian is src and dest in non-host byte order?
 */
void
pcm_unpack_24(int32_t *dest, const uint8_t *src, unsigned num_samples,
	      bool reverse_endian);

#endif
