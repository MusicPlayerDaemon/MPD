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

#include "Dsd32.hxx"
#include "util/ConstBuffer.hxx"

#include <functional>

/**
 * Construct a 32 bit integer from four bytes.
 */
static constexpr uint32_t
Construct32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept
{
	/* "a" is the oldest byte, which must be in the most
	   significant byte */

	return uint32_t(d) | (uint32_t(c) << 8) |
		(uint32_t(b) << 16) | (uint32_t(a) << 24);
}

static constexpr uint32_t
Dsd8To32Sample(const uint8_t *src, unsigned channels) noexcept
{
	return Construct32(src[0], src[channels],
			   src[2 * channels], src[3 * channels]);
}

static void
Dsd8To32(uint32_t *dest, const uint8_t *src,
	 size_t out_frames, unsigned channels) noexcept
{
	for (size_t i = 0; i < out_frames; ++i) {
		for (size_t c = 0; c < channels; ++c)
			*dest++ = Dsd8To32Sample(src++, channels);

		src += 3 * channels;
	}
}

void
Dsd32Converter::Open(unsigned _channels) noexcept
{
	channels = _channels;

	rest_buffer.Open(channels);
}

ConstBuffer<uint32_t>
Dsd32Converter::Convert(ConstBuffer<uint8_t> src) noexcept
{
	return rest_buffer.Process<uint32_t>(buffer, src, channels,
					     [=](auto && arg1, auto && arg2, auto && arg3) { return Dsd8To32(arg1, arg2, arg3, channels); });
}
