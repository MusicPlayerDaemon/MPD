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

#ifndef MPD_AUDIO_FORMAT_HXX
#define MPD_AUDIO_FORMAT_HXX

#include "Compiler.h"

#include <stdint.h>
#include <assert.h>

#if defined(WIN32) && GCC_CHECK_VERSION(4,6)
/* on WIN32, "FLOAT" is already defined, and this triggers -Wshadow */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

enum class SampleFormat : uint8_t {
	UNDEFINED = 0,

	S8,
	S16,

	/**
	 * Signed 24 bit integer samples, packed in 32 bit integers
	 * (the most significant byte is filled with the sign bit).
	 */
	S24_P32,

	S32,

	/**
	 * 32 bit floating point samples in the host's format.  The
	 * range is -1.0f to +1.0f.
	 */
	FLOAT,

	/**
	 * Direct Stream Digital.  1-bit samples; each frame has one
	 * byte (8 samples) per channel.
	 */
	DSD,
};

#if defined(WIN32) && GCC_CHECK_VERSION(4,6)
#pragma GCC diagnostic pop
#endif

static constexpr unsigned MAX_CHANNELS = 8;

/**
 * This structure describes the format of a raw PCM stream.
 */
struct AudioFormat {
	/**
	 * The sample rate in Hz.  A better name for this attribute is
	 * "frame rate", because technically, you have two samples per
	 * frame in stereo sound.
	 */
	uint32_t sample_rate;

	/**
	 * The format samples are stored in.  See the #sample_format
	 * enum for valid values.
	 */
	SampleFormat format;

	/**
	 * The number of channels.  Only mono (1) and stereo (2) are
	 * fully supported currently.
	 */
	uint8_t channels;

	AudioFormat() = default;

	constexpr AudioFormat(uint32_t _sample_rate,
			      SampleFormat _format, uint8_t _channels)
		:sample_rate(_sample_rate),
		 format(_format), channels(_channels) {}

	static constexpr AudioFormat Undefined() {
		return AudioFormat(0, SampleFormat::UNDEFINED,0);
	}

	/**
	 * Clears the #audio_format object, i.e. sets all attributes to an
	 * undefined (invalid) value.
	 */
	void Clear() {
		sample_rate = 0;
		format = SampleFormat::UNDEFINED;
		channels = 0;
	}

	/**
	 * Checks whether the object has a defined value.
	 */
	constexpr bool IsDefined() const {
		return sample_rate != 0;
	}

	/**
	 * Checks whether the object is full, i.e. all attributes are
	 * defined.  This is more complete than IsDefined(), but
	 * slower.
	 */
	constexpr bool IsFullyDefined() const {
		return sample_rate != 0 && format != SampleFormat::UNDEFINED &&
			channels != 0;
	}

	/**
	 * Checks whether the object has at least one defined value.
	 */
	constexpr bool IsMaskDefined() const {
		return sample_rate != 0 || format != SampleFormat::UNDEFINED ||
			channels != 0;
	}

	bool IsValid() const;
	bool IsMaskValid() const;

	constexpr bool operator==(const AudioFormat other) const {
		return sample_rate == other.sample_rate &&
			format == other.format &&
			channels == other.channels;
	}

	constexpr bool operator!=(const AudioFormat other) const {
		return !(*this == other);
	}

	void ApplyMask(AudioFormat mask);

	/**
	 * Returns the size of each (mono) sample in bytes.
	 */
	unsigned GetSampleSize() const;

	/**
	 * Returns the size of each full frame in bytes.
	 */
	unsigned GetFrameSize() const;

	/**
	 * Returns the floating point factor which converts a time
	 * span to a storage size in bytes.
	 */
	double GetTimeToSize() const;
};

/**
 * Buffer for audio_format_string().
 */
struct audio_format_string {
	char buffer[24];
};

/**
 * Checks whether the sample rate is valid.
 *
 * @param sample_rate the sample rate in Hz
 */
static constexpr inline bool
audio_valid_sample_rate(unsigned sample_rate)
{
	return sample_rate > 0 && sample_rate < (1 << 30);
}

/**
 * Checks whether the sample format is valid.
 *
 * @param bits the number of significant bits per sample
 */
static inline bool
audio_valid_sample_format(SampleFormat format)
{
	switch (format) {
	case SampleFormat::S8:
	case SampleFormat::S16:
	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
	case SampleFormat::DSD:
		return true;

	case SampleFormat::UNDEFINED:
		break;
	}

	return false;
}

/**
 * Checks whether the number of channels is valid.
 */
static constexpr inline bool
audio_valid_channel_count(unsigned channels)
{
	return channels >= 1 && channels <= MAX_CHANNELS;
}

/**
 * Returns false if the format is not valid for playback with MPD.
 * This function performs some basic validity checks.
 */
inline bool
AudioFormat::IsValid() const
{
	return audio_valid_sample_rate(sample_rate) &&
		audio_valid_sample_format(format) &&
		audio_valid_channel_count(channels);
}

/**
 * Returns false if the format mask is not valid for playback with
 * MPD.  This function performs some basic validity checks.
 */
inline bool
AudioFormat::IsMaskValid() const
{
	return (sample_rate == 0 ||
		audio_valid_sample_rate(sample_rate)) &&
		(format == SampleFormat::UNDEFINED ||
		 audio_valid_sample_format(format)) &&
		(channels == 0 || audio_valid_channel_count(channels));
}

gcc_const
static inline unsigned
sample_format_size(SampleFormat format)
{
	switch (format) {
	case SampleFormat::S8:
		return 1;

	case SampleFormat::S16:
		return 2;

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
		return 4;

	case SampleFormat::DSD:
		/* each frame has 8 samples per channel */
		return 1;

	case SampleFormat::UNDEFINED:
		return 0;
	}

	assert(false);
	gcc_unreachable();
}

inline unsigned
AudioFormat::GetSampleSize() const
{
	return sample_format_size(format);
}

inline unsigned
AudioFormat::GetFrameSize() const
{
	return GetSampleSize() * channels;
}

inline double
AudioFormat::GetTimeToSize() const
{
	return sample_rate * GetFrameSize();
}

/**
 * Renders a #sample_format enum into a string, e.g. for printing it
 * in a log file.
 *
 * @param format a #sample_format enum value
 * @return the string
 */
gcc_pure gcc_malloc
const char *
sample_format_to_string(SampleFormat format);

/**
 * Renders the #audio_format object into a string, e.g. for printing
 * it in a log file.
 *
 * @param af the #audio_format object
 * @param s a buffer to print into
 * @return the string, or nullptr if the #audio_format object is invalid
 */
gcc_pure gcc_malloc
const char *
audio_format_to_string(AudioFormat af,
		       struct audio_format_string *s);

#endif
