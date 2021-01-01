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

#include "Dsd16.hxx"
#include "util/ConstBuffer.hxx"

#include <functional>

/**
 * Construct a 16 bit integer from two bytes.
 */
static constexpr uint16_t
Construct16(uint8_t a, uint8_t b) noexcept
{
	/* "a" is the oldest byte, which must be in the most
	   significant byte */

	return uint16_t(b) | (uint16_t(a) << 8);
}

static constexpr uint16_t
Dsd8To16Sample(const uint8_t *src, unsigned channels) noexcept
{
	return Construct16(src[0], src[channels]);
}

static void
Dsd8To16(uint16_t *dest, const uint8_t *src,
	 size_t out_frames, unsigned channels) noexcept
{
	for (size_t i = 0; i < out_frames; ++i) {
		for (size_t c = 0; c < channels; ++c)
			*dest++ = Dsd8To16Sample(src++, channels);

		src += channels;
	}
}

void
Dsd16Converter::Open(unsigned _channels) noexcept
{
	channels = _channels;

	rest_buffer.Open(channels);
}

ConstBuffer<uint16_t>
Dsd16Converter::Convert(ConstBuffer<uint8_t> src) noexcept
{
	return rest_buffer.Process<uint16_t>(buffer, src, channels,
					     [=](auto && arg1, auto && arg2, auto && arg3) { return Dsd8To16(arg1, arg2, arg3, channels); });
}
