/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "PcmResampleInternal.hxx"

#include <assert.h>

/* resampling code blatantly ripped from ESD */
const int16_t *
pcm_resample_fallback_16(PcmResampler *state,
			 unsigned channels,
			 unsigned src_rate,
			 const int16_t *src_buffer, size_t src_size,
			 unsigned dest_rate,
			 size_t *dest_size_r)
{
	unsigned dest_pos = 0;
	unsigned src_frames = src_size / channels / sizeof(*src_buffer);
	unsigned dest_frames =
		(src_frames * dest_rate + src_rate - 1) / src_rate;
	unsigned dest_samples = dest_frames * channels;
	size_t dest_size = dest_samples * sizeof(*src_buffer);
	int16_t *dest_buffer = (int16_t *)state->buffer.Get(dest_size);

	assert((src_size % (sizeof(*src_buffer) * channels)) == 0);

	switch (channels) {
	case 1:
		while (dest_pos < dest_samples) {
			unsigned src_pos = dest_pos * src_rate / dest_rate;

			dest_buffer[dest_pos++] = src_buffer[src_pos];
		}
		break;
	case 2:
		while (dest_pos < dest_samples) {
			unsigned src_pos = dest_pos * src_rate / dest_rate;
			src_pos &= ~1;

			dest_buffer[dest_pos++] = src_buffer[src_pos];
			dest_buffer[dest_pos++] = src_buffer[src_pos + 1];
		}
		break;
	}

	*dest_size_r = dest_size;
	return dest_buffer;
}

const int32_t *
pcm_resample_fallback_32(PcmResampler *state,
			 unsigned channels,
			 unsigned src_rate,
			 const int32_t *src_buffer, size_t src_size,
			 unsigned dest_rate,
			 size_t *dest_size_r)
{
	unsigned dest_pos = 0;
	unsigned src_frames = src_size / channels / sizeof(*src_buffer);
	unsigned dest_frames =
		(src_frames * dest_rate + src_rate - 1) / src_rate;
	unsigned dest_samples = dest_frames * channels;
	size_t dest_size = dest_samples * sizeof(*src_buffer);
	int32_t *dest_buffer = (int32_t *)state->buffer.Get(dest_size);

	assert((src_size % (sizeof(*src_buffer) * channels)) == 0);

	switch (channels) {
	case 1:
		while (dest_pos < dest_samples) {
			unsigned src_pos = dest_pos * src_rate / dest_rate;

			dest_buffer[dest_pos++] = src_buffer[src_pos];
		}
		break;
	case 2:
		while (dest_pos < dest_samples) {
			unsigned src_pos = dest_pos * src_rate / dest_rate;
			src_pos &= ~1;

			dest_buffer[dest_pos++] = src_buffer[src_pos];
			dest_buffer[dest_pos++] = src_buffer[src_pos + 1];
		}
		break;
	}

	*dest_size_r = dest_size;
	return dest_buffer;
}
