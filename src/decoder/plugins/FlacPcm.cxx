// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FlacPcm.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "lib/xiph/FlacAudioFormat.hxx"
#include "lib/fmt/RuntimeError.hxx"

#include <cassert>

void
FlacPcmImport::Open(unsigned sample_rate, unsigned bits_per_sample,
		    unsigned channels)
{
	auto sample_format = FlacSampleFormat(bits_per_sample);
	if (sample_format == SampleFormat::UNDEFINED)
		throw FmtRuntimeError("Unsupported FLAC bit depth: {}",
				      bits_per_sample);

	audio_format = CheckAudioFormat(sample_rate, sample_format, channels);
}

template<typename T>
static void
FlacImportStereo(T *dest, const FLAC__int32 *const src[],
		 size_t n_frames) noexcept
{
	for (size_t i = 0; i != n_frames; ++i) {
		*dest++ = (T)src[0][i];
		*dest++ = (T)src[1][i];
	}
}

template<typename T>
static void
FlacImportAny(T *dest, const FLAC__int32 *const src[], size_t n_frames,
	      unsigned n_channels) noexcept
{
	for (size_t i = 0; i != n_frames; ++i)
		for (unsigned c = 0; c != n_channels; ++c)
			*dest++ = src[c][i];
}

template<typename T>
static void
FlacImport(T *dest, const FLAC__int32 *const src[], size_t n_frames,
	   unsigned n_channels) noexcept
{
	if (n_channels == 2)
		FlacImportStereo(dest, src, n_frames);
	else
		FlacImportAny(dest, src, n_frames, n_channels);
}

template<typename T>
static std::span<const std::byte>
FlacImport(PcmBuffer &buffer, const FLAC__int32 *const src[], size_t n_frames,
	   unsigned n_channels) noexcept
{
	size_t n_samples = n_frames * n_channels;
	size_t dest_size = n_samples * sizeof(T);
	T *dest = (T *)buffer.Get(dest_size);
	FlacImport(dest, src, n_frames, n_channels);
	return std::as_bytes(std::span{dest, n_samples});
}

std::span<const std::byte>
FlacPcmImport::Import(const FLAC__int32 *const src[], size_t n_frames) noexcept
{
	switch (audio_format.format) {
	case SampleFormat::S16:
		return FlacImport<int16_t>(buffer, src, n_frames,
					   audio_format.channels);

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
		return FlacImport<int32_t>(buffer, src, n_frames,
					   audio_format.channels);

	case SampleFormat::S8:
		return FlacImport<int8_t>(buffer, src, n_frames,
					  audio_format.channels);

	case SampleFormat::FLOAT:
	case SampleFormat::DSD:
	case SampleFormat::UNDEFINED:
		break;
	}

	assert(false);
	gcc_unreachable();
}
