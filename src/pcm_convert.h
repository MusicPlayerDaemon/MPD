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

#ifndef PCM_CONVERT_H
#define PCM_CONVERT_H

#include "pcm_dsd.h"
#include "pcm_resample.h"
#include "pcm_dither.h"
#include "pcm_buffer.h"

struct audio_format;

/**
 * This object is statically allocated (within another struct), and
 * holds buffer allocations and the state for all kinds of PCM
 * conversions.
 */
struct pcm_convert_state {
	struct pcm_dsd dsd;

	struct pcm_resample_state resample;

	struct pcm_dither dither;

	/** the buffer for converting the sample format */
	struct pcm_buffer format_buffer;

	/** the buffer for converting the channel count */
	struct pcm_buffer channels_buffer;
};

static inline GQuark
pcm_convert_quark(void)
{
	return g_quark_from_static_string("pcm_convert");
}

/**
 * Initializes a pcm_convert_state object.
 */
void pcm_convert_init(struct pcm_convert_state *state);

/**
 * Deinitializes a pcm_convert_state object and frees allocated
 * memory.
 */
void pcm_convert_deinit(struct pcm_convert_state *state);

/**
 * Reset the pcm_convert_state object.  Use this at the boundary
 * between two distinct songs and each time the format changes.
 */
void
pcm_convert_reset(struct pcm_convert_state *state);

/**
 * Converts PCM data between two audio formats.
 *
 * @param state an initialized pcm_convert_state object
 * @param src_format the source audio format
 * @param src the source PCM buffer
 * @param src_size the size of #src in bytes
 * @param dest_format the requested destination audio format
 * @param dest_size_r returns the number of bytes of the destination buffer
 * @param error_r location to store the error occurring, or NULL to
 * ignore errors
 * @return the destination buffer, or NULL on error
 */
const void *
pcm_convert(struct pcm_convert_state *state,
	    const struct audio_format *src_format,
	    const void *src, size_t src_size,
	    const struct audio_format *dest_format,
	    size_t *dest_size_r,
	    GError **error_r);

#endif
