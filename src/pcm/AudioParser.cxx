// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Parser functions for audio related objects.
 *
 */

#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "lib/fmt/RuntimeError.hxx"

#include <cassert>

#include <string.h>
#include <stdlib.h>

static uint32_t
ParseSampleRate(const char *src, bool mask, const char **endptr_r)
{
	unsigned long value;
	char *endptr;

	if (mask && *src == '*') {
		*endptr_r = src + 1;
		return 0;
	}

	value = strtoul(src, &endptr, 10);
	if (endptr == src) {
		throw std::invalid_argument("Failed to parse the sample rate");
	} else if (!audio_valid_sample_rate(value))
		throw FmtInvalidArgument("Invalid sample rate: {}", value);

	*endptr_r = endptr;
	return value;
}

static SampleFormat
ParseSampleFormat(const char *src, bool mask, const char **endptr_r)
{
	unsigned long value;
	char *endptr;
	SampleFormat sample_format;

	if (mask && *src == '*') {
		*endptr_r = src + 1;
		return SampleFormat::UNDEFINED;
	}

	if (*src == 'f') {
		*endptr_r = src + 1;
		return SampleFormat::FLOAT;
	}

	if (memcmp(src, "dsd", 3) == 0) {
		*endptr_r = src + 3;
		return SampleFormat::DSD;
	}

	value = strtoul(src, &endptr, 10);
	if (endptr == src)
		throw std::invalid_argument("Failed to parse the sample format");

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
		throw FmtInvalidArgument("Invalid sample format: {}", value);
	}

	assert(audio_valid_sample_format(sample_format));

	*endptr_r = endptr;
	return sample_format;
}

static uint8_t
ParseChannelCount(const char *src, bool mask, const char **endptr_r)
{
	unsigned long value;
	char *endptr;

	if (mask && *src == '*') {
		*endptr_r = src + 1;
		return 0;
	}

	value = strtoul(src, &endptr, 10);
	if (endptr == src)
		throw std::invalid_argument("Failed to parse the channel count");
	else if (!audio_valid_channel_count(value))
		throw FmtInvalidArgument("Invalid channel count: {}", value);

	*endptr_r = endptr;
	return value;
}

AudioFormat
ParseAudioFormat(const char *src, bool mask)
{
	AudioFormat dest;
	dest.Clear();

	if (strncmp(src, "dsd", 3) == 0) {
		/* allow format specifications such as "dsd64" which
		   implies the sample rate */

		char *endptr;
		auto dsd = strtoul(src + 3, &endptr, 10);
		if (endptr > src + 3 && *endptr == ':' &&
		    dsd >= 32 && dsd <= 4096 && dsd % 2 == 0) {
			dest.sample_rate = dsd * 44100 / 8;
			dest.format = SampleFormat::DSD;

			src = endptr + 1;
			dest.channels = ParseChannelCount(src, mask, &src);
			if (*src != 0)
				throw FmtInvalidArgument("Extra data after channel count: {}",
							 src);

			return dest;
		}
	}

	/* parse sample rate */

	dest.sample_rate = ParseSampleRate(src, mask, &src);

	if (*src++ != ':')
		throw std::invalid_argument("Sample format missing");

	/* parse sample format */

	dest.format = ParseSampleFormat(src, mask, &src);

	if (*src++ != ':')
		throw std::invalid_argument("Channel count missing");

	/* parse channel count */

	dest.channels = ParseChannelCount(src, mask, &src);

	if (*src != 0)
		throw FmtInvalidArgument("Extra data after channel count: {}",
					 src);

	assert(mask
	       ? dest.IsMaskValid()
	       : dest.IsValid());
	return dest;
}
