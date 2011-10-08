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

#ifndef PCM_FORMAT_H
#define PCM_FORMAT_H

#include "audio_format.h"

#include <stdint.h>
#include <stddef.h>

struct pcm_buffer;
struct pcm_dither;

/**
 * Converts PCM samples to 16 bit.  If the source format is 24 bit,
 * then dithering is applied.
 *
 * @param buffer a pcm_buffer object
 * @param dither a pcm_dither object for 24-to-16 conversion
 * @param bits the number of in the source buffer
 * @param src the source PCM buffer
 * @param src_size the size of #src in bytes
 * @param dest_size_r returns the number of bytes of the destination buffer
 * @return the destination buffer
 */
const int16_t *
pcm_convert_to_16(struct pcm_buffer *buffer, struct pcm_dither *dither,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r);

/**
 * Converts PCM samples to 24 bit (32 bit alignment).
 *
 * @param buffer a pcm_buffer object
 * @param bits the number of in the source buffer
 * @param src the source PCM buffer
 * @param src_size the size of #src in bytes
 * @param dest_size_r returns the number of bytes of the destination buffer
 * @return the destination buffer
 */
const int32_t *
pcm_convert_to_24(struct pcm_buffer *buffer,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r);

/**
 * Converts PCM samples to 32 bit.
 *
 * @param buffer a pcm_buffer object
 * @param bits the number of in the source buffer
 * @param src the source PCM buffer
 * @param src_size the size of #src in bytes
 * @param dest_size_r returns the number of bytes of the destination buffer
 * @return the destination buffer
 */
const int32_t *
pcm_convert_to_32(struct pcm_buffer *buffer,
		  enum sample_format src_format, const void *src,
		  size_t src_size, size_t *dest_size_r);

/**
 * Converts PCM samples to 32 bit floating point.
 *
 * @param buffer a pcm_buffer object
 * @param bits the number of in the source buffer
 * @param src the source PCM buffer
 * @param src_size the size of #src in bytes
 * @param dest_size_r returns the number of bytes of the destination buffer
 * @return the destination buffer
 */
const float *
pcm_convert_to_float(struct pcm_buffer *buffer,
		     enum sample_format src_format, const void *src,
		     size_t src_size, size_t *dest_size_r);

#endif
