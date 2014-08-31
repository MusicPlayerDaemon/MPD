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

#include "config.h"
#include "PcmDop.hxx"
#include "PcmBuffer.hxx"
#include "AudioFormat.hxx"
#include "util/ConstBuffer.hxx"

#include <assert.h>

constexpr
static inline uint32_t
pcm_two_dsd_to_dop_marker1(uint8_t a, uint8_t b)
{
	return 0xff050000 | (a << 8) | b;
}

constexpr
static inline uint32_t
pcm_two_dsd_to_dop_marker2(uint8_t a, uint8_t b)
{
	return 0xfffa0000 | (a << 8) | b;
}

ConstBuffer<uint32_t>
pcm_dsd_to_dop(PcmBuffer &buffer, unsigned channels,
	       ConstBuffer<uint8_t> _src)
{
	assert(audio_valid_channel_count(channels));
	assert(!_src.IsNull());
	assert(_src.size > 0);
	assert(_src.size % channels == 0);

	const unsigned num_src_samples = _src.size;
	const unsigned num_src_frames = num_src_samples / channels;

	/* this rounds down and discards the last odd frame; not
	   elegant, but good enough for now */
	const unsigned num_frames = num_src_frames / 2;
	const unsigned num_samples = num_frames * channels;

	uint32_t *const dest0 = (uint32_t *)buffer.GetT<uint32_t>(num_samples),
		*dest = dest0;

	auto src = _src.data;
	for (unsigned i = num_frames / 2; i > 0; --i) {
		for (unsigned c = channels; c > 0; --c) {
			/* each 24 bit sample has 16 DSD sample bits
			   plus the magic 0x05 marker */

			*dest++ = pcm_two_dsd_to_dop_marker1(src[0], src[channels]);

			/* seek the source pointer to the next
			   channel */
			++src;
		}

		/* skip the second byte of each channel, because we
		   have already copied it */
		src += channels;

		for (unsigned c = channels; c > 0; --c) {
			/* each 24 bit sample has 16 DSD sample bits
			   plus the magic 0xfa marker */

			*dest++ = pcm_two_dsd_to_dop_marker2(src[0], src[channels]);

			/* seek the source pointer to the next
			   channel */
			++src;
		}

		/* skip the second byte of each channel, because we
		   have already copied it */
		src += channels;
	}

	return { dest0, num_samples };
}
