/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#include <glib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

enum sample_format {
	SAMPLE_FORMAT_UNDEFINED = 0,

	SAMPLE_FORMAT_S8,
	SAMPLE_FORMAT_S16,

	/**
	 * Signed 24 bit integer samples, packed in 32 bit integers
	 * (the most significant byte is filled with the sign bit).
	 */
	SAMPLE_FORMAT_S24_P32,

	SAMPLE_FORMAT_S32,

	/**
	 * 32 bit floating point samples in the host's format.  The
	 * range is -1.0f to +1.0f.
	 */
	SAMPLE_FORMAT_FLOAT,

	/**
	 * Direct Stream Digital.  1-bit samples; each frame has one
	 * byte (8 samples) per channel.
	 */
	SAMPLE_FORMAT_DSD,
};

static const unsigned MAX_CHANNELS = 8;

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
	 * The format samples are stored in.  See the #sample_format
	 * enum for valid values.
	 */
	uint8_t format;

	/**
	 * The number of channels.  Only mono (1) and stereo (2) are
	 * fully supported currently.
	 */
	uint8_t channels;
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
	af->format = SAMPLE_FORMAT_UNDEFINED;
	af->channels = 0;
}

/**
 * Initializes an #audio_format object, i.e. sets all
 * attributes to valid values.
 */
static inline void audio_format_init(struct audio_format *af,
				     uint32_t sample_rate,
				     enum sample_format format, uint8_t channels)
{
	af->sample_rate = sample_rate;
	af->format = (uint8_t)format;
	af->channels = channels;
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
	return af->sample_rate != 0 && af->format != SAMPLE_FORMAT_UNDEFINED &&
		af->channels != 0;
}

/**
 * Checks whether the specified #audio_format object has at least one
 * defined value.
 */
static inline bool
audio_format_mask_defined(const struct audio_format *af)
{
	return af->sample_rate != 0 || af->format != SAMPLE_FORMAT_UNDEFINED ||
		af->channels != 0;
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
audio_valid_sample_format(enum sample_format format)
{
	switch (format) {
	case SAMPLE_FORMAT_S8:
	case SAMPLE_FORMAT_S16:
	case SAMPLE_FORMAT_S24_P32:
	case SAMPLE_FORMAT_S32:
	case SAMPLE_FORMAT_FLOAT:
	case SAMPLE_FORMAT_DSD:
		return true;

	case SAMPLE_FORMAT_UNDEFINED:
		break;
	}

	return false;
}

/**
 * Checks whether the number of channels is valid.
 */
static inline bool
audio_valid_channel_count(unsigned channels)
{
	return channels >= 1 && channels <= MAX_CHANNELS;
}

/**
 * Returns false if the format is not valid for playback with MPD.
 * This function performs some basic validity checks.
 */
G_GNUC_PURE
static inline bool audio_format_valid(const struct audio_format *af)
{
	return audio_valid_sample_rate(af->sample_rate) &&
		audio_valid_sample_format((enum sample_format)af->format) &&
		audio_valid_channel_count(af->channels);
}

/**
 * Returns false if the format mask is not valid for playback with
 * MPD.  This function performs some basic validity checks.
 */
G_GNUC_PURE
static inline bool audio_format_mask_valid(const struct audio_format *af)
{
	return (af->sample_rate == 0 ||
		audio_valid_sample_rate(af->sample_rate)) &&
		(af->format == SAMPLE_FORMAT_UNDEFINED ||
		 audio_valid_sample_format((enum sample_format)af->format)) &&
		(af->channels == 0 || audio_valid_channel_count(af->channels));
}

static inline bool audio_format_equals(const struct audio_format *a,
				       const struct audio_format *b)
{
	return a->sample_rate == b->sample_rate &&
		a->format == b->format &&
		a->channels == b->channels;
}

void
audio_format_mask_apply(struct audio_format *af,
			const struct audio_format *mask);

G_GNUC_CONST
static inline unsigned
sample_format_size(enum sample_format format)
{
	switch (format) {
	case SAMPLE_FORMAT_S8:
		return 1;

	case SAMPLE_FORMAT_S16:
		return 2;

	case SAMPLE_FORMAT_S24_P32:
	case SAMPLE_FORMAT_S32:
	case SAMPLE_FORMAT_FLOAT:
		return 4;

	case SAMPLE_FORMAT_DSD:
		/* each frame has 8 samples per channel */
		return 1;

	case SAMPLE_FORMAT_UNDEFINED:
		return 0;
	}

	assert(false);
	return 0;
}

/**
 * Returns the size of each (mono) sample in bytes.
 */
G_GNUC_PURE
static inline unsigned audio_format_sample_size(const struct audio_format *af)
{
	return sample_format_size((enum sample_format)af->format);
}

/**
 * Returns the size of each full frame in bytes.
 */
G_GNUC_PURE
static inline unsigned
audio_format_frame_size(const struct audio_format *af)
{
	return audio_format_sample_size(af) * af->channels;
}

/**
 * Returns the floating point factor which converts a time span to a
 * storage size in bytes.
 */
G_GNUC_PURE
static inline double audio_format_time_to_size(const struct audio_format *af)
{
	return af->sample_rate * audio_format_frame_size(af);
}

/**
 * Renders a #sample_format enum into a string, e.g. for printing it
 * in a log file.
 *
 * @param format a #sample_format enum value
 * @return the string
 */
G_GNUC_PURE G_GNUC_MALLOC
const char *
sample_format_to_string(enum sample_format format);

/**
 * Renders the #audio_format object into a string, e.g. for printing
 * it in a log file.
 *
 * @param af the #audio_format object
 * @param s a buffer to print into
 * @return the string, or NULL if the #audio_format object is invalid
 */
G_GNUC_PURE G_GNUC_MALLOC
const char *
audio_format_to_string(const struct audio_format *af,
		       struct audio_format_string *s);

#endif
