// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Dsd32.hxx"

/**
 * Construct a 32 bit integer from four bytes.
 */
static constexpr uint32_t
Construct32(std::byte a, std::byte b, std::byte c, std::byte d) noexcept
{
	/* "a" is the oldest byte, which must be in the most
	   significant byte */

	return uint32_t(d) | (uint32_t(c) << 8) |
		(uint32_t(b) << 16) | (uint32_t(a) << 24);
}

static constexpr uint32_t
Dsd8To32Sample(const std::byte *src, unsigned channels) noexcept
{
	return Construct32(src[0], src[channels],
			   src[2 * channels], src[3 * channels]);
}

static void
Dsd8To32(uint32_t *dest, const std::byte *src,
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

std::span<const uint32_t>
Dsd32Converter::Convert(std::span<const std::byte> src) noexcept
{
	return rest_buffer.Process<uint32_t>(buffer, src, channels,
					     [this](auto && arg1, auto && arg2, auto && arg3) { return Dsd8To32(arg1, arg2, arg3, channels); });
}
