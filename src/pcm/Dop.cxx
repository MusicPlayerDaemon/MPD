/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Dop.hxx"
#include "ChannelDefs.hxx"
#include "util/ConstBuffer.hxx"

#include <cassert>
#include <functional>

static constexpr uint32_t
pcm_two_dsd_to_dop_marker1(uint8_t a, uint8_t b) noexcept
{
	return 0xff050000 | (a << 8) | b;
}

static constexpr uint32_t
pcm_two_dsd_to_dop_marker2(uint8_t a, uint8_t b) noexcept
{
	return 0xfffa0000 | (a << 8) | b;
}

/**
 * @param num_dop_quads the number of "quad" bytes per channel in the
 * source buffer; each "quad" will be converted to two 24 bit samples
 * in the destination buffer, one for each marker
 */
static void
DsdToDop(uint32_t *dest, const uint8_t *src,
	 size_t num_dop_quads, unsigned channels) noexcept
{
	for (size_t i = num_dop_quads; i > 0; --i) {
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
}

void
DsdToDopConverter::Open(unsigned _channels) noexcept
{
	assert(audio_valid_channel_count(_channels));

	channels = _channels;

	rest_buffer.Open(channels);
}

ConstBuffer<uint32_t>
DsdToDopConverter::Convert(ConstBuffer<uint8_t> src) noexcept
{
	return rest_buffer.Process<uint32_t>(buffer, src, 2 * channels,
		[=](auto && arg1, auto && arg2, auto && arg3) { return DsdToDop(arg1, arg2, arg3, channels); });
}
