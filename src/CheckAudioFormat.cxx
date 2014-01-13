/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "CheckAudioFormat.hxx"
#include "AudioFormat.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <assert.h>

const Domain audio_format_domain("audio_format");

bool
audio_check_sample_rate(unsigned long sample_rate, Error &error)
{
	if (!audio_valid_sample_rate(sample_rate)) {
		error.Format(audio_format_domain,
			     "Invalid sample rate: %lu", sample_rate);
		return false;
	}

	return true;
}

bool
audio_check_sample_format(SampleFormat sample_format, Error &error)
{
	if (!audio_valid_sample_format(sample_format)) {
		error.Format(audio_format_domain,
			     "Invalid sample format: %u",
			     unsigned(sample_format));
		return false;
	}

	return true;
}

bool
audio_check_channel_count(unsigned channels, Error &error)
{
	if (!audio_valid_channel_count(channels)) {
		error.Format(audio_format_domain,
			     "Invalid channel count: %u", channels);
		return false;
	}

	return true;
}

bool
audio_format_init_checked(AudioFormat &af, unsigned long sample_rate,
			  SampleFormat sample_format, unsigned channels,
			  Error &error)
{
	if (audio_check_sample_rate(sample_rate, error) &&
	    audio_check_sample_format(sample_format, error) &&
	    audio_check_channel_count(channels, error)) {
		af = AudioFormat(sample_rate, sample_format, channels);
		assert(af.IsValid());
		return true;
	} else
		return false;
}
