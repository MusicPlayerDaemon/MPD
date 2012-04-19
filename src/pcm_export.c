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
#include "pcm_dsd_usb.h"
#include "pcm_pack.h"
#include "util/byte_reverse.h"

void
pcm_export_init(struct pcm_export_state *state)
{
	pcm_buffer_init(&state->reverse_buffer);
	pcm_buffer_init(&state->pack_buffer);
	pcm_buffer_init(&state->dsd_buffer);
}

void pcm_export_deinit(struct pcm_export_state *state)
{
	pcm_buffer_deinit(&state->reverse_buffer);
	pcm_buffer_deinit(&state->pack_buffer);
	pcm_buffer_deinit(&state->dsd_buffer);
}

void
pcm_export_open(struct pcm_export_state *state,
		enum sample_format sample_format, unsigned channels,
		bool dsd_usb, bool shift8, bool pack, bool reverse_endian)
{
	assert(audio_valid_sample_format(sample_format));
	assert(!dsd_usb || audio_valid_channel_count(channels));

	state->channels = channels;
	state->dsd_usb = dsd_usb && sample_format == SAMPLE_FORMAT_DSD;
	if (state->dsd_usb)
		/* after the conversion to DSD-over-USB, the DSD
		   samples are stuffed inside fake 24 bit samples */
		sample_format = SAMPLE_FORMAT_S24_P32;

	state->shift8 = shift8 && sample_format == SAMPLE_FORMAT_S24_P32;
	state->pack24 = pack && sample_format == SAMPLE_FORMAT_S24_P32;

	assert(!state->shift8 || !state->pack24);

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

size_t
pcm_export_frame_size(const struct pcm_export_state *state,
		      const struct audio_format *audio_format)
{
	assert(state != NULL);
	assert(audio_format != NULL);

	if (state->pack24)
		/* packed 24 bit samples (3 bytes per sample) */
		return audio_format->channels * 3;

	if (state->dsd_usb)
		/* the DSD-over-USB draft says that DSD 1-bit samples
		   are enclosed within 24 bit samples, and MPD's
		   representation of 24 bit is padded to 32 bit (4
		   bytes per sample) */
		return audio_format->channels * 4;

	return audio_format_frame_size(audio_format);
}

const void *
pcm_export(struct pcm_export_state *state, const void *data, size_t size,
	   size_t *dest_size_r)
{
	if (state->dsd_usb)
		data = pcm_dsd_to_usb(&state->dsd_buffer, state->channels,
				      data, size, &size);

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
	} else if (state->shift8) {
		assert(size % 4 == 0);

		const uint8_t *src8 = data, *src_end8 = src8 + size;
		const uint32_t *src = (const uint32_t *)src8;
		const uint32_t *const src_end = (const uint32_t *)src_end8;

		uint32_t *dest = pcm_buffer_get(&state->pack_buffer, size);
		data = dest;

		while (src < src_end)
			*dest++ = *src++ << 8;
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

size_t
pcm_export_source_size(const struct pcm_export_state *state, size_t size)
{
	if (state->pack24)
		/* 32 bit to 24 bit conversion (4 to 3 bytes) */
		size = (size / 3) * 4;

	if (state->dsd_usb)
		/* DSD over USB doubles the transport size */
		size /= 2;

	return size;
}
