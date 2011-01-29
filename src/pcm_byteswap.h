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

#ifndef MPD_PCM_BYTESWAP_H
#define MPD_PCM_BYTESWAP_H

#include <stdint.h>
#include <stddef.h>

struct pcm_buffer;

/**
 * Changes the endianness of 16 bit PCM data.
 *
 * @param buffer the destination pcm_buffer object
 * @param src the source PCM buffer
 * @param src_size the number of bytes in #src
 * @return the destination buffer
 */
const int16_t *pcm_byteswap_16(struct pcm_buffer *buffer,
			       const int16_t *src, size_t len);

/**
 * Changes the endianness of 32-bit (or 24-bit) PCM data.
 *
 * @param buffer the destination pcm_buffer object
 * @param src the source PCM buffer
 * @param src_size the number of bytes in #src
 * @return the destination buffer
 */
const int32_t *pcm_byteswap_32(struct pcm_buffer *buffer,
			       const int32_t *src, size_t len);

#endif
