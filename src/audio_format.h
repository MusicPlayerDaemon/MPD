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

#ifndef MPD_AUDIO_FORMAT_H
#define MPD_AUDIO_FORMAT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * This structure describes the format of a raw PCM stream.
 */
struct audio_format {
	/**
	 * The sample rate in Hz.  A better name for this attribute is
	 * "frame rate", because technically, you have two samples per
	 * frame in stereo sound.
	 */
	uint32_t sample_rate;

	/**
	 * The number of significant bits per sample.  Samples are
	 * currently always signed.  Supported values are 8, 16, 24,
	 * 32.  24 bit samples are packed in 32 bit integers.
	 */
	uint8_t bits;

	/**
	 * The number of channels.  Only mono (1) and stereo (2) are
	 * fully supported currently.
	 */
	uint8_t channels;

	/**
	 * If zero, then samples are stored in host byte order.  If
	 * nonzero, then samples are stored in the reverse host byte
	 * order.
	 */
	uint8_t reverse_endian;
};

/**
 * Buffer for audio_format_string().
 */
struct audio_format_string {
	char buffer[24];
};

/**
 * Clears the #audio_format object, i.e. sets all attributes to an
 * undefined (invalid) value.
 */
static inline void audio_format_clear(struct audio_format *af)
{
	af->sample_rate = 0;
	af->bits = 0;
	af->channels = 0;
	af->reverse_endian = 0;
}

/**
 * Initializes an #audio_format object, i.e. sets all
 * attributes to valid values.
 */
static inline void audio_format_init(struct audio_format *af,
				     uint32_t sample_rate,
				     uint8_t bits, uint8_t channels)
{
	af->sample_rate = sample_rate;
	af->bits = bits;
	af->channels = channels;
	af->reverse_endian = 0;
}

/**
 * Checks whether the specified #audio_format object has a defined
 * value.
 */
static inline bool audio_format_defined(const struct audio_format *af)
{
	return af->sample_rate != 0;
}

/**
 * Checks whether the specified #audio_format object is full, i.e. all
 * attributes are defined.  This is more complete than
 * audio_format_defined(), but slower.
 */
static inline bool
audio_format_fully_defined(const struct audio_format *af)
{
	return af->sample_rate != 0 && af->bits != 0 && af->channels != 0;
}

/**
 * Checks whether the specified #audio_format object has at least one
 * defined value.
 */
static inline bool
audio_format_mask_defined(const struct audio_format *af)
{
	return af->sample_rate != 0 || af->bits != 0 || af->channels != 0;
}

/**
 * Checks whether the sample rate is valid.
 *
 * @param sample_rate the sample rate in Hz
 */
static inline bool
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
audio_valid_sample_format(unsigned bits)
{
	return bits == 16 || bits == 24 || bits == 32 || bits == 8;
}

/**
 * Checks whether the number of channels is valid.
 */
static inline bool
audio_valid_channel_count(unsigned channels)
{
	return channels >= 1 && channels <= 8;
}

/**
 * Returns false if the format is not valid for playback with MPD.
 * This function performs some basic validity checks.
 */
static inline bool audio_format_valid(const struct audio_format *af)
{
	return audio_valid_sample_rate(af->sample_rate) &&
		audio_valid_sample_format(af->bits) &&
		audio_valid_channel_count(af->channels);
}

/**
 * Returns false if the format mask is not valid for playback with
 * MPD.  This function performs some basic validity checks.
 */
static inline bool audio_format_mask_valid(const struct audio_format *af)
{
	return (af->sample_rate == 0 ||
		audio_valid_sample_rate(af->sample_rate)) &&
		(af->bits == 0 || audio_valid_sample_format(af->bits)) &&
		(af->channels == 0 || audio_valid_channel_count(af->channels));
}

static inline bool audio_format_equals(const struct audio_format *a,
				       const struct audio_format *b)
{
	return a->sample_rate == b->sample_rate &&
		a->bits == b->bits &&
		a->channels == b->channels &&
		a->reverse_endian == b->reverse_endian;
}

static inline void
audio_format_mask_apply(struct audio_format *af,
			const struct audio_format *mask)
{
	if (mask->sample_rate != 0)
		af->sample_rate = mask->sample_rate;

	if (mask->bits != 0)
		af->bits = mask->bits;

	if (mask->channels != 0)
		af->channels = mask->channels;
}

/**
 * Returns the size of each (mono) sample in bytes.
 */
static inline unsigned audio_format_sample_size(const struct audio_format *af)
{
	if (af->bits <= 8)
		return 1;
	else if (af->bits <= 16)
		return 2;
	else
		return 4;
}

/**
 * Returns the size of each full frame in bytes.
 */
static inline unsigned
audio_format_frame_size(const struct audio_format *af)
{
	return audio_format_sample_size(af) * af->channels;
}

/**
 * Returns the floating point factor which converts a time span to a
 * storage size in bytes.
 */
static inline double audio_format_time_to_size(const struct audio_format *af)
{
	return af->sample_rate * audio_format_frame_size(af);
}

/**
 * Renders the #audio_format object into a string, e.g. for printing
 * it in a log file.
 *
 * @param af the #audio_format object
 * @param s a buffer to print into
 * @return the string, or NULL if the #audio_format object is invalid
 */
const char *
audio_format_to_string(const struct audio_format *af,
		       struct audio_format_string *s);

#endif
