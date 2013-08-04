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

/** \file
 *
 * Internal declarations for the pcm_resample library.  The "internal"
 * resampler is called "fallback" in the MPD source, so the file name
 * of this header is somewhat unrelated to it.
 */

#ifndef MPD_PCM_RESAMPLE_INTERNAL_HXX
#define MPD_PCM_RESAMPLE_INTERNAL_HXX

#include "check.h"
#include "PcmResample.hxx"

#ifdef HAVE_LIBSAMPLERATE

bool
pcm_resample_lsr_global_init(const char *converter, GError **error_r);

void
pcm_resample_lsr_init(PcmResampler *state);

void
pcm_resample_lsr_deinit(PcmResampler *state);

void
pcm_resample_lsr_reset(PcmResampler *state);

const float *
pcm_resample_lsr_float(PcmResampler *state,
		       unsigned channels,
		       unsigned src_rate,
		       const float *src_buffer, size_t src_size,
		       unsigned dest_rate, size_t *dest_size_r,
		       GError **error_r);

const int16_t *
pcm_resample_lsr_16(PcmResampler *state,
		    unsigned channels,
		    unsigned src_rate,
		    const int16_t *src_buffer, size_t src_size,
		    unsigned dest_rate, size_t *dest_size_r,
		    GError **error_r);

const int32_t *
pcm_resample_lsr_32(PcmResampler *state,
		    unsigned channels,
		    unsigned src_rate,
		    const int32_t *src_buffer,
		    size_t src_size,
		    unsigned dest_rate, size_t *dest_size_r,
		    GError **error_r);

#endif

const int16_t *
pcm_resample_fallback_16(PcmResampler *state,
			 unsigned channels,
			 unsigned src_rate,
			 const int16_t *src_buffer, size_t src_size,
			 unsigned dest_rate,
			 size_t *dest_size_r);

const int32_t *
pcm_resample_fallback_32(PcmResampler *state,
			 unsigned channels,
			 unsigned src_rate,
			 const int32_t *src_buffer,
			 size_t src_size,
			 unsigned dest_rate,
			 size_t *dest_size_r);

#endif
