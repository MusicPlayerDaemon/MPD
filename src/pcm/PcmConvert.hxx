/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef PCM_CONVERT_HXX
#define PCM_CONVERT_HXX

#include "PcmDither.hxx"
#include "PcmDsd.hxx"
#include "PcmResample.hxx"

extern "C" {
#include "pcm_buffer.h"
}

#include <glib.h>

struct audio_format;

/**
 * This object is statically allocated (within another struct), and
 * holds buffer allocations and the state for all kinds of PCM
 * conversions.
 */
class PcmConvert {
	PcmDsd dsd;

	PcmResampler resampler;

	PcmDither dither;

	/** the buffer for converting the sample format */
	struct pcm_buffer format_buffer;

	/** the buffer for converting the channel count */
	struct pcm_buffer channels_buffer;

public:
	PcmConvert();
	~PcmConvert();


	/**
	 * Reset the pcm_convert_state object.  Use this at the
	 * boundary between two distinct songs and each time the
	 * format changes.
	 */
	void Reset();

	/**
	 * Converts PCM data between two audio formats.
	 *
	 * @param src_format the source audio format
	 * @param src the source PCM buffer
	 * @param src_size the size of #src in bytes
	 * @param dest_format the requested destination audio format
	 * @param dest_size_r returns the number of bytes of the destination buffer
	 * @param error_r location to store the error occurring, or NULL to
	 * ignore errors
	 * @return the destination buffer, or NULL on error
	 */
	const void *Convert(const audio_format *src_format,
			    const void *src, size_t src_size,
			    const audio_format *dest_format,
			    size_t *dest_size_r,
			    GError **error_r);

private:
	const int16_t *Convert16(const audio_format *src_format,
				 const void *src_buffer, size_t src_size,
				 const audio_format *dest_format,
				 size_t *dest_size_r,
				 GError **error_r);

	const int32_t *Convert24(const audio_format *src_format,
				 const void *src_buffer, size_t src_size,
				 const audio_format *dest_format,
				 size_t *dest_size_r,
				 GError **error_r);

	const int32_t *Convert32(const audio_format *src_format,
				 const void *src_buffer, size_t src_size,
				 const audio_format *dest_format,
				 size_t *dest_size_r,
				 GError **error_r);

	const float *ConvertFloat(const audio_format *src_format,
				  const void *src_buffer, size_t src_size,
				  const audio_format *dest_format,
				  size_t *dest_size_r,
				  GError **error_r);
};

static inline GQuark
pcm_convert_quark(void)
{
	return g_quark_from_static_string("pcm_convert");
}

#endif
