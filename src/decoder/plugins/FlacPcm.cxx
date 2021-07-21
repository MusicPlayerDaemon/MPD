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

#include "FlacPcm.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "lib/xiph/FlacAudioFormat.hxx"
#include "util/RuntimeError.hxx"
#include "util/ConstBuffer.hxx"

#include <cassert>

void
FlacPcmImport::Open(unsigned sample_rate, unsigned bits_per_sample,
		    unsigned channels)
{
	auto sample_format = FlacSampleFormat(bits_per_sample);
	if (sample_format == SampleFormat::UNDEFINED)
		throw FormatRuntimeError("Unsupported FLAC bit depth: %u",
					 bits_per_sample);

	audio_format = CheckAudioFormat(sample_rate, sample_format, channels);
}

template<typename T>
static void
FlacImportStereo(T *dest, const FLAC__int32 *const src[], size_t n_frames)
{
	for (size_t i = 0; i != n_frames; ++i) {
		*dest++ = (T)src[0][i];
		*dest++ = (T)src[1][i];
	}
}

template<typename T>
static void
FlacImportAny(T *dest, const FLAC__int32 *const src[], size_t n_frames,
	      unsigned n_channels)
{
	for (size_t i = 0; i != n_frames; ++i)
		for (unsigned c = 0; c != n_channels; ++c)
			*dest++ = src[c][i];
}

template<typename T>
static void
FlacImport(T *dest, const FLAC__int32 *const src[], size_t n_frames,
	   unsigned n_channels)
{
	if (n_channels == 2)
		FlacImportStereo(dest, src, n_frames);
	else
		FlacImportAny(dest, src, n_frames, n_channels);
}

template<typename T>
static ConstBuffer<void>
FlacImport(PcmBuffer &buffer, const FLAC__int32 *const src[], size_t n_frames,
	   unsigned n_channels)
{
	size_t n_samples = n_frames * n_channels;
	size_t dest_size = n_samples * sizeof(T);
	T *dest = (T *)buffer.Get(dest_size);
	FlacImport(dest, src, n_frames, n_channels);
	return {dest, dest_size};
}

ConstBuffer<void>
FlacPcmImport::Import(const FLAC__int32 *const src[], size_t n_frames)
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
