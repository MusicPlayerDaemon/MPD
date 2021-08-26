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

#ifndef MPD_PCM_SAMPLE_FORMAT_HXX
#define MPD_PCM_SAMPLE_FORMAT_HXX

#include "util/Compiler.h"

#include <cstdint>

#if defined(_WIN32)
/* on WIN32, "FLOAT" is already defined, and this triggers -Wshadow */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

enum class SampleFormat : uint8_t {
	UNDEFINED = 0,

	S8,
	S16,

	/**
	 * Signed 24 bit integer samples, packed in 32 bit integers
	 * (the most significant byte is filled with the sign bit).
	 */
	S24_P32,

	S32,

	/**
	 * 32 bit floating point samples in the host's format.  The
	 * range is -1.0f to +1.0f.
	 */
	FLOAT,

	/**
	 * Direct Stream Digital.  1-bit samples; each frame has one
	 * byte (8 samples) per channel.
	 */
	DSD,
};

#if defined(_WIN32)
#pragma GCC diagnostic pop
#endif

/**
 * Checks whether the sample format is valid.
 */
constexpr bool
audio_valid_sample_format(SampleFormat format) noexcept
{
	switch (format) {
	case SampleFormat::S8:
	case SampleFormat::S16:
	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
	case SampleFormat::DSD:
		return true;

	case SampleFormat::UNDEFINED:
		break;
	}

	return false;
}

constexpr unsigned
sample_format_size(SampleFormat format) noexcept
{
	switch (format) {
	case SampleFormat::S8:
		return 1;

	case SampleFormat::S16:
		return 2;

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
		return 4;

	case SampleFormat::DSD:
		/* each frame has 8 samples per channel */
		return 1;

	case SampleFormat::UNDEFINED:
		return 0;
	}

	gcc_unreachable();
}

/**
 * Renders a #SampleFormat enum into a string, e.g. for printing it
 * in a log file.
 *
 * @param format a #SampleFormat enum value
 * @return the string
 */
[[gnu::pure]] [[gnu::returns_nonnull]]
const char *
sample_format_to_string(SampleFormat format) noexcept;

#endif
