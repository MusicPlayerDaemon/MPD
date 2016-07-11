/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "FlacPcm.hxx"

#include <assert.h>

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
	FlacImportAny(dest, src, n_frames, n_channels);
}

void
flac_convert(void *dest,
	     unsigned int num_channels, SampleFormat sample_format,
	     const FLAC__int32 *const buf[],
	     size_t n_frames)
{
	switch (sample_format) {
	case SampleFormat::S16:
		if (num_channels == 2)
			FlacImportStereo((int16_t *)dest, buf, n_frames);
		else
			FlacImportAny((int16_t *)dest, buf, n_frames, num_channels);
		break;

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
		FlacImport((int32_t *)dest, buf, n_frames, num_channels);
		break;

	case SampleFormat::S8:
		FlacImport((int8_t *)dest, buf, n_frames, num_channels);
		break;

	case SampleFormat::FLOAT:
	case SampleFormat::DSD:
	case SampleFormat::UNDEFINED:
		assert(false);
		gcc_unreachable();
	}
}
