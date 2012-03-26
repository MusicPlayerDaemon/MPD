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
#include "pcm_export.h"
#include "pcm_pack.h"
#include "util/byte_reverse.h"

void
pcm_export_init(struct pcm_export_state *state)
{
	pcm_buffer_init(&state->reverse_buffer);
	pcm_buffer_init(&state->pack_buffer);
}

void pcm_export_deinit(struct pcm_export_state *state)
{
	pcm_buffer_deinit(&state->reverse_buffer);
	pcm_buffer_deinit(&state->pack_buffer);
}

void
pcm_export_open(struct pcm_export_state *state,
		enum sample_format sample_format,
		bool pack, bool reverse_endian)
{
	state->pack24 = pack && sample_format == SAMPLE_FORMAT_S24_P32;

	state->reverse_endian = 0;
	if (reverse_endian) {
		size_t sample_size = state->pack24
			? 3
			: sample_format_size(sample_format);
		assert(sample_size <= 0xff);

		if (sample_size > 1)
			state->reverse_endian = sample_size;
	}
}

const void *
pcm_export(struct pcm_export_state *state, const void *data, size_t size,
	   size_t *dest_size_r)
{
	if (state->pack24) {
		assert(size % 4 == 0);

		const size_t num_samples = size / 4;
		const size_t dest_size = num_samples * 3;

		const uint8_t *src8 = data, *src_end8 = src8 + size;
		uint8_t *dest = pcm_buffer_get(&state->pack_buffer, dest_size);
		assert(dest != NULL);

		pcm_pack_24(dest, (const int32_t *)src8,
			    (const int32_t *)src_end8);

		data = dest;
		size = dest_size;
	}

	if (state->reverse_endian > 0) {
		assert(state->reverse_endian >= 2);

		void *dest = pcm_buffer_get(&state->reverse_buffer, size);
		assert(dest != NULL);

		const uint8_t *src = data, *src_end = src + size;
		reverse_bytes(dest, src, src_end, state->reverse_endian);

		data = dest;
	}

	*dest_size_r = size;
	return data;
}
