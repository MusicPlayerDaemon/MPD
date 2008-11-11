/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_PCM_UTILS_H
#define MPD_PCM_UTILS_H

#include "pcm_resample.h"
#include "pcm_dither.h"

#include <stdint.h>
#include <stddef.h>

struct audio_format;

struct pcm_convert_state {
	struct pcm_resample_state resample;

	struct pcm_dither_24 dither;

	/* Strict C99 doesn't allow empty structs */
	int error;
};

/**
 * Converts a float value (0.0 = silence, 1.0 = 100% volume) to an
 * integer volume value (1000 = 100%).
 */
static inline int
pcm_float_to_volume(float volume)
{
	return volume * 1000.0 + 0.5;
}

void pcm_volume(char *buffer, int bufferSize,
		const struct audio_format *format,
		int volume);

void pcm_mix(char *buffer1, const char *buffer2, size_t size,
             const struct audio_format *format, float portion1);

void pcm_convert_init(struct pcm_convert_state *state);

size_t pcm_convert(const struct audio_format *inFormat,
		   const char *inBuffer, size_t inSize,
		   const struct audio_format *outFormat,
		   char *outBuffer,
		   struct pcm_convert_state *convState);

size_t pcm_convert_size(const struct audio_format *inFormat, size_t inSize,
			const struct audio_format *outFormat);

#endif
