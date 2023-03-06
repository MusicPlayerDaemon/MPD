// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "CheckAudioFormat.hxx"
#include "AudioFormat.hxx"
#include "lib/fmt/RuntimeError.hxx"

void
CheckSampleRate(unsigned long sample_rate)
{
	if (!audio_valid_sample_rate(sample_rate))
		throw FmtRuntimeError("Invalid sample rate: {}",
				      sample_rate);
}

void
CheckSampleFormat(SampleFormat sample_format)
{
	if (!audio_valid_sample_format(sample_format))
		throw FmtRuntimeError("Invalid sample format: {}",
				      unsigned(sample_format));
}

void
CheckChannelCount(unsigned channels)
{
	if (!audio_valid_channel_count(channels))
		throw FmtRuntimeError("Invalid channel count: {}",
				      channels);
}

AudioFormat
CheckAudioFormat(unsigned long sample_rate,
		 SampleFormat sample_format, unsigned channels)
{
	CheckSampleRate(sample_rate);
	CheckSampleFormat(sample_format);
	CheckChannelCount(channels);

	return AudioFormat(sample_rate, sample_format, channels);
}
