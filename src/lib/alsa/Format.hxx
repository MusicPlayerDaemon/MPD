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

#ifndef MPD_ALSA_FORMAT_HXX
#define MPD_ALSA_FORMAT_HXX

#include "pcm/SampleFormat.hxx"
#include "util/Compiler.h"
#include "config.h"

#include <alsa/asoundlib.h>

#include <cassert>

#if SND_LIB_VERSION >= 0x1001c
/* alsa-lib supports DSD since version 1.0.27.1 */
#define HAVE_ALSA_DSD
#endif

#if SND_LIB_VERSION >= 0x1001d
/* alsa-lib supports DSD_U32 since version 1.0.29 */
#define HAVE_ALSA_DSD_U32
#endif

/**
 * Convert MPD's #SampleFormat enum to libasound's snd_pcm_format_t
 * enum.  Returns SND_PCM_FORMAT_UNKNOWN if there is no according ALSA
 * PCM format.
 */
[[gnu::const]]
inline snd_pcm_format_t
ToAlsaPcmFormat(SampleFormat sample_format) noexcept
{
	switch (sample_format) {
	case SampleFormat::UNDEFINED:
		return SND_PCM_FORMAT_UNKNOWN;

	case SampleFormat::DSD:
#ifdef HAVE_ALSA_DSD
		return SND_PCM_FORMAT_DSD_U8;
#else
		return SND_PCM_FORMAT_UNKNOWN;
#endif

	case SampleFormat::S8:
		return SND_PCM_FORMAT_S8;

	case SampleFormat::S16:
		return SND_PCM_FORMAT_S16;

	case SampleFormat::S24_P32:
		return SND_PCM_FORMAT_S24;

	case SampleFormat::S32:
		return SND_PCM_FORMAT_S32;

	case SampleFormat::FLOAT:
		return SND_PCM_FORMAT_FLOAT;
	}

	assert(false);
	gcc_unreachable();
}

/**
 * Determine the byte-swapped PCM format.  Returns
 * SND_PCM_FORMAT_UNKNOWN if the format cannot be byte-swapped.
 */
[[gnu::const]]
inline snd_pcm_format_t
ByteSwapAlsaPcmFormat(snd_pcm_format_t fmt) noexcept
{
	switch (fmt) {
	case SND_PCM_FORMAT_S16_LE: return SND_PCM_FORMAT_S16_BE;
	case SND_PCM_FORMAT_S24_LE: return SND_PCM_FORMAT_S24_BE;
	case SND_PCM_FORMAT_S32_LE: return SND_PCM_FORMAT_S32_BE;
	case SND_PCM_FORMAT_S16_BE: return SND_PCM_FORMAT_S16_LE;
	case SND_PCM_FORMAT_S24_BE: return SND_PCM_FORMAT_S24_LE;

	case SND_PCM_FORMAT_S24_3BE:
		return SND_PCM_FORMAT_S24_3LE;

	case SND_PCM_FORMAT_S24_3LE:
		return SND_PCM_FORMAT_S24_3BE;

	case SND_PCM_FORMAT_S32_BE: return SND_PCM_FORMAT_S32_LE;

#ifdef HAVE_ALSA_DSD_U32
	case SND_PCM_FORMAT_DSD_U16_LE:
		return SND_PCM_FORMAT_DSD_U16_BE;

	case SND_PCM_FORMAT_DSD_U16_BE:
		return SND_PCM_FORMAT_DSD_U16_LE;

	case SND_PCM_FORMAT_DSD_U32_LE:
		return SND_PCM_FORMAT_DSD_U32_BE;

	case SND_PCM_FORMAT_DSD_U32_BE:
		return SND_PCM_FORMAT_DSD_U32_LE;
#endif

	default: return SND_PCM_FORMAT_UNKNOWN;
	}
}

/**
 * Check if there is a "packed" version of the give PCM format.
 * Returns SND_PCM_FORMAT_UNKNOWN if not.
 */
constexpr snd_pcm_format_t
PackAlsaPcmFormat(snd_pcm_format_t fmt) noexcept
{
	switch (fmt) {
	case SND_PCM_FORMAT_S24_LE:
		return SND_PCM_FORMAT_S24_3LE;

	case SND_PCM_FORMAT_S24_BE:
		return SND_PCM_FORMAT_S24_3BE;

	default:
		return SND_PCM_FORMAT_UNKNOWN;
	}
}

#endif
