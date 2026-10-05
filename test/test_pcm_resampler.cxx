// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "pcm/FallbackResampler.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>

TEST(PcmTest, FallbackResamplerMultiChannel)
{
	constexpr unsigned channels = 3;
	constexpr unsigned src_frames = 4;

	AudioFormat af{44100, SampleFormat::S16, channels};

	FallbackPcmResampler resampler;
	resampler.Open(af, 88200);

	std::array<int16_t, src_frames * channels> src;
	for (std::size_t i = 0; i < src.size(); ++i)
		src[i] = int16_t(i + 1);

	const auto dest = FromBytesStrict<const int16_t>(resampler.Resample(std::as_bytes(std::span{src})));
	ASSERT_EQ(dest.size(), 2 * src.size());

	/* each output frame must be a copy of a source frame */
	for (std::size_t frame = 0; frame < dest.size() / channels; ++frame) {
		const std::size_t src_frame = frame / 2;
		for (unsigned c = 0; c < channels; ++c)
			EXPECT_EQ(dest[frame * channels + c],
				  src[src_frame * channels + c]);
	}

	resampler.Close();
}
