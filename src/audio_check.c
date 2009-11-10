/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "audio_check.h"
#include "audio_format.h"

#include <assert.h>

bool
audio_check_sample_rate(unsigned long sample_rate, GError **error_r)
{
	if (!audio_valid_sample_rate(sample_rate)) {
		g_set_error(error_r, audio_format_quark(), 0,
			    "Invalid sample rate: %lu", sample_rate);
		return false;
	}

	return true;
}

bool
audio_check_sample_format(unsigned sample_format, GError **error_r)
{
	if (!audio_valid_sample_format(sample_format)) {
		g_set_error(error_r, audio_format_quark(), 0,
			    "Invalid sample format: %u", sample_format);
		return false;
	}

	return true;
}

bool
audio_check_channel_count(unsigned channels, GError **error_r)
{
	if (!audio_valid_channel_count(channels)) {
		g_set_error(error_r, audio_format_quark(), 0,
			    "Invalid channel count: %u", channels);
		return false;
	}

	return true;
}

bool
audio_format_init_checked(struct audio_format *af, unsigned long sample_rate,
			  unsigned sample_format, unsigned channels,
			  GError **error_r)
{
	if (audio_check_sample_rate(sample_rate, error_r) &&
	    audio_check_sample_format(sample_format, error_r) &&
	    audio_check_channel_count(channels, error_r)) {
		audio_format_init(af, sample_rate, sample_format, channels);
		assert(audio_format_valid(af));
		return true;
	} else
		return false;
}
