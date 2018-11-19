/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "PcmBuffer.hxx"
#include "util/ConstBuffer.hxx"

/**
 * Construct a 16 bit integer from two bytes.
 */
static constexpr inline uint16_t
Construct16(uint8_t a, uint8_t b) noexcept
{
	/* "a" is the oldest byte, which must be in the most
	   significant byte */

	return uint16_t(b) | (uint16_t(a) << 8);
}

static constexpr inline uint16_t
Dsd8To16Sample(const uint8_t *src, unsigned channels) noexcept
{
	return Construct16(src[0], src[channels]);
}

ConstBuffer<uint16_t>
Dsd8To16(PcmBuffer &buffer, unsigned channels,
	 ConstBuffer<uint8_t> _src) noexcept
{
	const size_t in_frames = _src.size / channels;
	const size_t out_frames = in_frames / 2;
	const size_t out_samples = out_frames * channels;

	const uint8_t *src = _src.data;
	uint16_t *const dest0 = buffer.GetT<uint16_t>(out_samples);
	uint16_t *dest = dest0;

	for (size_t i = 0; i < out_frames; ++i) {
		for (size_t c = 0; c < channels; ++c)
			*dest++ = Dsd8To16Sample(src++, channels);

		src += channels;
	}

	return {dest0, out_samples};
}
