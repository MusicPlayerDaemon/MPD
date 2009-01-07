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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef PCM_CONVERT_H
#define PCM_CONVERT_H

#include "pcm_resample.h"
#include "pcm_dither.h"
#include "pcm_buffer.h"

struct audio_format;

struct pcm_convert_state {
	struct pcm_resample_state resample;

	struct pcm_dither_24 dither;

	/** the buffer for converting the sample format */
	struct pcm_buffer format_buffer;

	/** the buffer for converting the channel count */
	struct pcm_buffer channels_buffer;
};

void pcm_convert_init(struct pcm_convert_state *state);

void pcm_convert_deinit(struct pcm_convert_state *state);

size_t pcm_convert(const struct audio_format *inFormat,
		   const void *src, size_t src_size,
		   const struct audio_format *outFormat,
		   void *dest,
		   struct pcm_convert_state *convState);

size_t pcm_convert_size(const struct audio_format *inFormat, size_t inSize,
			const struct audio_format *outFormat);

#endif
