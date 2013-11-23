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

#ifndef MPD_PCM_RESAMPLE_HXX
#define MPD_PCM_RESAMPLE_HXX

#include "check.h"
#include "PcmBuffer.hxx"

#include <stdint.h>
#include <stddef.h>

#ifdef HAVE_LIBSAMPLERATE
#include <samplerate.h>
#endif

class Error;

/**
 * This object is statically allocated (within another struct), and
 * holds buffer allocations and the state for the resampler.
 */
struct PcmResampler {
#ifdef HAVE_LIBSAMPLERATE
	SRC_STATE *state;
	SRC_DATA data;

	PcmBuffer in, out;

	struct {
		unsigned src_rate;
		unsigned dest_rate;
		unsigned channels;
	} prev;

	int error;
#endif

	PcmBuffer buffer;

	PcmResampler();
	~PcmResampler();

	/**
	 * @see pcm_convert_reset()
	 */
	void Reset();

	/**
	 * Resamples 32 bit float data.
	 *
	 * @param channels the number of channels
	 * @param src_rate the source sample rate
	 * @param src the source PCM buffer
	 * @param src_size the size of #src in bytes
	 * @param dest_rate the requested destination sample rate
	 * @param dest_size_r returns the number of bytes of the destination buffer
	 * @return the destination buffer
	 */
	const float *ResampleFloat(unsigned channels, unsigned src_rate,
				   const float *src_buffer, size_t src_size,
				   unsigned dest_rate, size_t *dest_size_r,
				   Error &error_r);

	/**
	 * Resamples 16 bit PCM data.
	 *
	 * @param channels the number of channels
	 * @param src_rate the source sample rate
	 * @param src the source PCM buffer
	 * @param src_size the size of #src in bytes
	 * @param dest_rate the requested destination sample rate
	 * @param dest_size_r returns the number of bytes of the destination buffer
	 * @return the destination buffer
	 */
	const int16_t *Resample16(unsigned channels, unsigned src_rate,
				  const int16_t *src_buffer, size_t src_size,
				  unsigned dest_rate, size_t *dest_size_r,
				  Error &error_r);

	/**
	 * Resamples 32 bit PCM data.
	 *
	 * @param channels the number of channels
	 * @param src_rate the source sample rate
	 * @param src the source PCM buffer
	 * @param src_size the size of #src in bytes
	 * @param dest_rate the requested destination sample rate
	 * @param dest_size_r returns the number of bytes of the destination buffer
	 * @return the destination buffer
	 */
	const int32_t *Resample32(unsigned channels, unsigned src_rate,
				  const int32_t *src_buffer, size_t src_size,
				  unsigned dest_rate, size_t *dest_size_r,
				  Error &error_r);

	/**
	 * Resamples 24 bit PCM data.
	 *
	 * @param channels the number of channels
	 * @param src_rate the source sample rate
	 * @param src the source PCM buffer
	 * @param src_size the size of #src in bytes
	 * @param dest_rate the requested destination sample rate
	 * @param dest_size_r returns the number of bytes of the destination buffer
	 * @return the destination buffer
	 */
	const int32_t *Resample24(unsigned channels, unsigned src_rate,
				  const int32_t *src_buffer, size_t src_size,
				  unsigned dest_rate, size_t *dest_size_r,
				  Error &error_r);
};

bool
pcm_resample_global_init(Error &error);

#endif
