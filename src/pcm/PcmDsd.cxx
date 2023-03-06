// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PcmDsd.hxx"
#include "Dsd2Pcm.hxx"

#include <cassert>

std::span<const float>
PcmDsd::ToFloat(unsigned channels, std::span<const std::byte> src) noexcept
{
	assert(!src.empty());
	assert(src.size() % channels == 0);

	const size_t num_samples = src.size();
	const size_t num_frames = src.size() / channels;

	auto *dest = buffer.GetT<float>(num_samples);

	dsd2pcm.Translate(channels, num_frames, src.data(), dest);
	return { dest, num_samples };
}

std::span<const int32_t>
PcmDsd::ToS24(unsigned channels, std::span<const std::byte> src) noexcept
{
	assert(!src.empty());
	assert(src.size() % channels == 0);

	const size_t num_samples = src.size();
	const size_t num_frames = src.size() / channels;

	auto *dest = buffer.GetT<int32_t>(num_samples);

	dsd2pcm.TranslateS24(channels, num_frames, src.data(), dest);
	return { dest, num_samples };
}
