// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "pcm/SampleFormat.hxx"
#include "util/ByteOrder.hxx"

#include <tinyalsa/pcm.h>

#include <utility> // for std::unreachable()

namespace TinyAlsa {

/**
 * Convert MPD's #SampleFormat enum to tinyalsa's #pcm_format enum
 * (in host byte order).  Returns PCM_FORMAT_INVALID if there is no
 * according tinyalsa PCM format.
 */
[[gnu::const]]
constexpr enum pcm_format
ToPcmFormat(SampleFormat sample_format) noexcept
{
	switch (sample_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::FLOAT:
	case SampleFormat::DSD:
		/* not supported by tinyalsa 2.0 */
		return PCM_FORMAT_INVALID;

	case SampleFormat::S8:
		return PCM_FORMAT_S8;

	case SampleFormat::S16:
		return IsLittleEndian()
			? PCM_FORMAT_S16_LE
			: PCM_FORMAT_S16_BE;

	case SampleFormat::S24_P32:
		return IsLittleEndian()
			? PCM_FORMAT_S24_LE
			: PCM_FORMAT_S24_BE;

	case SampleFormat::S32:
		return IsLittleEndian()
			? PCM_FORMAT_S32_LE
			: PCM_FORMAT_S32_BE;
	}

	std::unreachable();
}

/**
 * Determine the byte-swapped PCM format.  Returns
 * PCM_FORMAT_INVALID if the format cannot be byte-swapped.
 */
[[gnu::const]]
constexpr enum pcm_format
ByteSwapPcmFormat(enum pcm_format format) noexcept
{
	switch (format) {
	case PCM_FORMAT_S16_LE: return PCM_FORMAT_S16_BE;
	case PCM_FORMAT_S16_BE: return PCM_FORMAT_S16_LE;
	case PCM_FORMAT_S24_LE: return PCM_FORMAT_S24_BE;
	case PCM_FORMAT_S24_BE: return PCM_FORMAT_S24_LE;
	case PCM_FORMAT_S24_3LE: return PCM_FORMAT_S24_3BE;
	case PCM_FORMAT_S24_3BE: return PCM_FORMAT_S24_3LE;
	case PCM_FORMAT_S32_LE: return PCM_FORMAT_S32_BE;
	case PCM_FORMAT_S32_BE: return PCM_FORMAT_S32_LE;
	default: return PCM_FORMAT_INVALID;
	}
}

/**
 * Check if there is a "packed" version of the given PCM format.
 * Returns PCM_FORMAT_INVALID if not.
 */
[[gnu::const]]
constexpr enum pcm_format
PackPcmFormat(enum pcm_format format) noexcept
{
	switch (format) {
	case PCM_FORMAT_S24_LE: return PCM_FORMAT_S24_3LE;
	case PCM_FORMAT_S24_BE: return PCM_FORMAT_S24_3BE;
	default: return PCM_FORMAT_INVALID;
	}
}

} // namespace TinyAlsa
