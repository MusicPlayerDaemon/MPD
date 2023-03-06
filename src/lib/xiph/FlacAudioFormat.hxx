// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FLAC_AUDIO_FORMAT_HXX
#define MPD_FLAC_AUDIO_FORMAT_HXX

#include "pcm/SampleFormat.hxx"

constexpr SampleFormat
FlacSampleFormat(unsigned bits_per_sample) noexcept
{
	switch (bits_per_sample) {
	case 8:
		return SampleFormat::S8;

	case 16:
		return SampleFormat::S16;

	case 24:
		return SampleFormat::S24_P32;

	case 32:
		return SampleFormat::S32;

	default:
		return SampleFormat::UNDEFINED;
	}
}

#endif
