// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "pcm/SampleFormat.hxx"
#include "util/Compiler.h"

#include <alsa/asoundlib.h>

#include <cassert>

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
		return SND_PCM_FORMAT_DSD_U8;

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

	case SND_PCM_FORMAT_DSD_U16_LE:
		return SND_PCM_FORMAT_DSD_U16_BE;

	case SND_PCM_FORMAT_DSD_U16_BE:
		return SND_PCM_FORMAT_DSD_U16_LE;

	case SND_PCM_FORMAT_DSD_U32_LE:
		return SND_PCM_FORMAT_DSD_U32_BE;

	case SND_PCM_FORMAT_DSD_U32_BE:
		return SND_PCM_FORMAT_DSD_U32_LE;

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
