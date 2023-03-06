// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Dsd16.hxx"

/**
 * Construct a 16 bit integer from two bytes.
 */
static constexpr uint16_t
Construct16(std::byte a, std::byte b) noexcept
{
	/* "a" is the oldest byte, which must be in the most
	   significant byte */

	return uint16_t(b) | (uint16_t(a) << 8);
}

static constexpr uint16_t
Dsd8To16Sample(const std::byte *src, unsigned channels) noexcept
{
	return Construct16(src[0], src[channels]);
}

static void
Dsd8To16(uint16_t *dest, const std::byte *src,
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

std::span<const uint16_t>
Dsd16Converter::Convert(std::span<const std::byte> src) noexcept
{
	return rest_buffer.Process<uint16_t>(buffer, src, channels,
					     [this](auto && arg1, auto && arg2, auto && arg3) { return Dsd8To16(arg1, arg2, arg3, channels); });
}
