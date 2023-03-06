// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CHECK_AUDIO_FORMAT_HXX
#define MPD_CHECK_AUDIO_FORMAT_HXX

#include "AudioFormat.hxx"

void
CheckSampleRate(unsigned long sample_rate);

void
CheckSampleFormat(SampleFormat sample_format);

void
CheckChannelCount(unsigned sample_format);

/**
 * Check #AudioFormat attributes and construct an #AudioFormat
 * instance.
 *
 * Throws #std::runtime_error on error.
 */
AudioFormat
CheckAudioFormat(unsigned long sample_rate,
		 SampleFormat sample_format, unsigned channels);

#endif
