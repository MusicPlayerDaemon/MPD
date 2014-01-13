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

/*
 * Parser functions for audio related objects.
 *
 */

#include "config.h"
#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "CheckAudioFormat.hxx"
#include "util/Error.hxx"
#include "Compiler.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static bool
parse_sample_rate(const char *src, bool mask, uint32_t *sample_rate_r,
		  const char **endptr_r, Error &error)
{
	unsigned long value;
	char *endptr;

	if (mask && *src == '*') {
		*sample_rate_r = 0;
		*endptr_r = src + 1;
		return true;
	}

	value = strtoul(src, &endptr, 10);
	if (endptr == src) {
		error.Set(audio_format_domain,
			  "Failed to parse the sample rate");
		return false;
	} else if (!audio_check_sample_rate(value, error))
		return false;

	*sample_rate_r = value;
	*endptr_r = endptr;
	return true;
}

static bool
parse_sample_format(const char *src, bool mask,
		    SampleFormat *sample_format_r,
		    const char **endptr_r, Error &error)
{
	unsigned long value;
	char *endptr;
	SampleFormat sample_format;

	if (mask && *src == '*') {
		*sample_format_r = SampleFormat::UNDEFINED;
		*endptr_r = src + 1;
		return true;
	}

	if (*src == 'f') {
		*sample_format_r = SampleFormat::FLOAT;
		*endptr_r = src + 1;
		return true;
	}

	if (memcmp(src, "dsd", 3) == 0) {
		*sample_format_r = SampleFormat::DSD;
		*endptr_r = src + 3;
		return true;
	}

	value = strtoul(src, &endptr, 10);
	if (endptr == src) {
		error.Set(audio_format_domain,
			  "Failed to parse the sample format");
		return false;
	}

	switch (value) {
	case 8:
		sample_format = SampleFormat::S8;
		break;

	case 16:
		sample_format = SampleFormat::S16;
		break;

	case 24:
		if (memcmp(endptr, "_3", 2) == 0)
			/* for backwards compatibility */
			endptr += 2;

		sample_format = SampleFormat::S24_P32;
		break;

	case 32:
		sample_format = SampleFormat::S32;
		break;

	default:
		error.Format(audio_format_domain,
			     "Invalid sample format: %lu", value);
		return false;
	}

	assert(audio_valid_sample_format(sample_format));

	*sample_format_r = sample_format;
	*endptr_r = endptr;
	return true;
}

static bool
parse_channel_count(const char *src, bool mask, uint8_t *channels_r,
		    const char **endptr_r, Error &error)
{
	unsigned long value;
	char *endptr;

	if (mask && *src == '*') {
		*channels_r = 0;
		*endptr_r = src + 1;
		return true;
	}

	value = strtoul(src, &endptr, 10);
	if (endptr == src) {
		error.Set(audio_format_domain,
			  "Failed to parse the channel count");
		return false;
	} else if (!audio_check_channel_count(value, error))
		return false;

	*channels_r = value;
	*endptr_r = endptr;
	return true;
}

bool
audio_format_parse(AudioFormat &dest, const char *src,
		   bool mask, Error &error)
{
	uint32_t rate;
	SampleFormat sample_format;
	uint8_t channels;

	dest.Clear();

	/* parse sample rate */

#if GCC_CHECK_VERSION(4,7)
	/* workaround -Wmaybe-uninitialized false positive */
	rate = 0;
#endif

	if (!parse_sample_rate(src, mask, &rate, &src, error))
		return false;

	if (*src++ != ':') {
		error.Set(audio_format_domain, "Sample format missing");
		return false;
	}

	/* parse sample format */

#if GCC_CHECK_VERSION(4,7)
	/* workaround -Wmaybe-uninitialized false positive */
	sample_format = SampleFormat::UNDEFINED;
#endif

	if (!parse_sample_format(src, mask, &sample_format, &src, error))
		return false;

	if (*src++ != ':') {
		error.Set(audio_format_domain, "Channel count missing");
		return false;
	}

	/* parse channel count */

	if (!parse_channel_count(src, mask, &channels, &src, error))
		return false;

	if (*src != 0) {
		error.Format(audio_format_domain,
			    "Extra data after channel count: %s", src);
		return false;
	}

	dest = AudioFormat(rate, sample_format, channels);
	assert(mask
	       ? dest.IsMaskValid()
	       : dest.IsValid());

	return true;
}
