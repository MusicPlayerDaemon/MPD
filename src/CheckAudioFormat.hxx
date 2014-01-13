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

#ifndef MPD_CHECK_AUDIO_FORMAT_HXX
#define MPD_CHECK_AUDIO_FORMAT_HXX

#include "AudioFormat.hxx"

class Error;

extern const class Domain audio_format_domain;

bool
audio_check_sample_rate(unsigned long sample_rate, Error &error);

bool
audio_check_sample_format(SampleFormat sample_format, Error &error);

bool
audio_check_channel_count(unsigned sample_format, Error &error);

/**
 * Wrapper for audio_format_init(), which checks all attributes.
 */
bool
audio_format_init_checked(AudioFormat &af, unsigned long sample_rate,
			  SampleFormat sample_format, unsigned channels,
			  Error &error);

#endif
