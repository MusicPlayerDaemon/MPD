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

#ifndef PCM_VOLUME_H
#define PCM_VOLUME_H

#include "pcm_prng.h"
#include "audio_format.h"

#include <stdint.h>
#include <stdbool.h>

enum {
	/** this value means "100% volume" */
	PCM_VOLUME_1 = 1024,
};

struct audio_format;

/**
 * Converts a float value (0.0 = silence, 1.0 = 100% volume) to an
 * integer volume value (1000 = 100%).
 */
static inline int
pcm_float_to_volume(float volume)
{
	return volume * PCM_VOLUME_1 + 0.5;
}

static inline float
pcm_volume_to_float(int volume)
{
	return (float)volume / (float)PCM_VOLUME_1;
}

/**
 * Returns the next volume dithering number, between -511 and +511.
 * This number is taken from a global PRNG, see pcm_prng().
 */
static inline int
pcm_volume_dither(void)
{
	static unsigned long state;
	uint32_t r;

	r = state = pcm_prng(state);

	return (r & 511) - ((r >> 9) & 511);
}

/**
 * Adjust the volume of the specified PCM buffer.
 *
 * @param buffer the PCM buffer
 * @param length the length of the PCM buffer
 * @param format the sample format of the PCM buffer
 * @param volume the volume between 0 and #PCM_VOLUME_1
 * @return true on success, false if the audio format is not supported
 */
bool
pcm_volume(void *buffer, size_t length,
	   enum sample_format format,
	   int volume);

#endif
