/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "Interleave.hxx"

#include <stdint.h>
#include <string.h>

static void
GenericPcmInterleave(uint8_t *dest, ConstBuffer<const uint8_t *> src,
		     size_t n_frames, size_t sample_size)
{
	for (size_t frame = 0; frame < n_frames; ++frame) {
		for (size_t channel = 0; channel < src.size; ++channel) {
			memcpy(dest, src[channel] + frame * sample_size,
			       sample_size);
			dest += sample_size;
		}
	}
}

void
PcmInterleave(void *dest, ConstBuffer<const void *> src,
	      size_t n_frames, size_t sample_size)
{
	GenericPcmInterleave((uint8_t *)dest,
			     ConstBuffer<const uint8_t *>((const uint8_t *const*)src.data,
							  src.size),
			     n_frames, sample_size);
}
