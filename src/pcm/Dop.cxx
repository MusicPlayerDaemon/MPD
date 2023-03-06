// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Dop.hxx"
#include "ChannelDefs.hxx"

#include <cassert>
#include <functional>

static constexpr uint32_t
pcm_two_dsd_to_dop_marker1(std::byte a, std::byte b) noexcept
{
	return 0xff050000 | (static_cast<uint32_t>(a) << 8) | static_cast<uint32_t>(b);
}

static constexpr uint32_t
pcm_two_dsd_to_dop_marker2(std::byte a, std::byte b) noexcept
{
	return 0xfffa0000 | (static_cast<uint32_t>(a) << 8) | static_cast<uint32_t>(b);
}

/**
 * @param num_dop_quads the number of "quad" bytes per channel in the
 * source buffer; each "quad" will be converted to two 24 bit samples
 * in the destination buffer, one for each marker
 */
static void
DsdToDop(uint32_t *dest, const std::byte *src,
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

std::span<const uint32_t>
DsdToDopConverter::Convert(std::span<const std::byte> src) noexcept
{
	return rest_buffer.Process<uint32_t>(buffer, src, 2 * channels,
					     [this](auto && arg1, auto && arg2, auto && arg3) { return DsdToDop(arg1, arg2, arg3, channels); });
}
