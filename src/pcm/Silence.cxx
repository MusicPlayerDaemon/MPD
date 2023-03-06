// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Silence.hxx"
#include "Traits.hxx"
#include "SampleFormat.hxx"

#include <algorithm>

void
PcmSilence(std::span<std::byte> dest, SampleFormat format) noexcept
{
	std::byte pattern{0};
	if (format == SampleFormat::DSD)
		pattern = std::byte{SampleTraits<SampleFormat::DSD>::SILENCE};

	std::fill(dest.begin(), dest.end(), pattern);
}
