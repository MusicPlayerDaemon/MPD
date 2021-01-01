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

#include "CheckAudioFormat.hxx"
#include "AudioFormat.hxx"
#include "util/RuntimeError.hxx"

void
CheckSampleRate(unsigned long sample_rate)
{
	if (!audio_valid_sample_rate(sample_rate))
		throw FormatRuntimeError("Invalid sample rate: %lu",
					 sample_rate);
}

void
CheckSampleFormat(SampleFormat sample_format)
{
	if (!audio_valid_sample_format(sample_format))
		throw FormatRuntimeError("Invalid sample format: %u",
					 unsigned(sample_format));
}

void
CheckChannelCount(unsigned channels)
{
	if (!audio_valid_channel_count(channels))
		throw FormatRuntimeError("Invalid channel count: %u",
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
