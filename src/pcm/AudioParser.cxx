// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Parser functions for audio related objects.
 *
 */

#include "AudioParser.hxx"
#include "AudioFormat.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/NumberParser.hxx"
#include "util/StringCompare.hxx"
#include "util/StringSplit.hxx"

#include <cassert>

using std::string_view_literals::operator""sv;

static uint32_t
ParseSampleRate(std::string_view src, bool mask)
{
	if (mask && src == "*"sv)
		return 0;

	const auto value = ParseInteger<uint32_t>(src);
	if (!value)
		throw std::invalid_argument("Failed to parse the sample rate");

	if (!audio_valid_sample_rate(*value))
		throw FmtInvalidArgument("Invalid sample rate: {}", *value);

	return *value;
}

static SampleFormat
ParseSampleFormat(std::string_view src, bool mask)
{
	if (mask && src == "*"sv)
		return SampleFormat::UNDEFINED;
	else if (src == "f"sv)
		return SampleFormat::FLOAT;
	else if (src == "dsd"sv)
		return SampleFormat::DSD;
	else if (src == "8"sv)
		return SampleFormat::S8;
	else if (src == "16"sv)
		return SampleFormat::S16;
	else if (src == "24"sv)
		return SampleFormat::S24_P32;
	else if (src == "24_3"sv)
		/* for backwards compatibility */
		return SampleFormat::S24_P32;
	else if (src == "32"sv)
		return SampleFormat::S32;
	else
		throw FmtInvalidArgument("Invalid sample format: {:?}", src);
}

static uint8_t
ParseChannelCount(std::string_view src, bool mask)
{
	if (mask && src == "*"sv)
		return 0;

	const auto value = ParseInteger<uint_least8_t>(src);
	if (!value)
		throw std::invalid_argument("Failed to parse the channel count");
	else if (!audio_valid_channel_count(*value))
		throw FmtInvalidArgument("Invalid channel count: {}", *value);

	return *value;
}

AudioFormat
ParseAudioFormat(std::string_view src, bool mask)
{
	AudioFormat dest;
	dest.Clear();

	if (SkipPrefix(src, "dsd"sv)) {
		/* allow format specifications such as "dsd64" which
		   implies the sample rate */

		const auto [dsd_s, channels_s] = Split(src, ':');
		if (channels_s.data() == nullptr)
			throw std::invalid_argument("Channel count missing");

		const auto dsd = ParseInteger<uint_least16_t>(dsd_s);
		if (!dsd)
			throw std::invalid_argument("Failed to parse the DSD rate");

		if (*dsd < 32 || *dsd > 4096)
			throw std::invalid_argument("Bad DSD rate");

		dest.sample_rate = *dsd * 44100 / 8;
		dest.format = SampleFormat::DSD;
		dest.channels = ParseChannelCount(channels_s, mask);
		return dest;
	}

	/* parse sample rate */

	const auto [sample_rate_s, rest1] = Split(src, ':');

	dest.sample_rate = ParseSampleRate(sample_rate_s, mask);

	/* parse sample format */

	if (rest1.data() == nullptr)
		throw std::invalid_argument("Sample format missing");

	const auto [format_s, channels_s] = Split(rest1, ':');

	dest.format = ParseSampleFormat(format_s, mask);

	/* parse channel count */

	if (channels_s.data() == nullptr)
		throw std::invalid_argument("Channel count missing");

	dest.channels = ParseChannelCount(channels_s, mask);

	assert(mask
	       ? dest.IsMaskValid()
	       : dest.IsValid());
	return dest;
}
