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

/** \file
 *
 * Internal declarations for the pcm_resample library.  The "internal"
 * resampler is called "fallback" in the MPD source, so the file name
 * of this header is somewhat unrelated to it.
 */

#ifndef MPD_PCM_RESAMPLE_INTERNAL_H
#define MPD_PCM_RESAMPLE_INTERNAL_H

#include "check.h"
#include "pcm_resample.h"

#ifdef HAVE_LIBSAMPLERATE

void
pcm_resample_lsr_deinit(struct pcm_resample_state *state);

const int16_t *
pcm_resample_lsr_16(struct pcm_resample_state *state,
		    uint8_t channels,
		    unsigned src_rate,
		    const int16_t *src_buffer, size_t src_size,
		    unsigned dest_rate, size_t *dest_size_r,
		    GError **error_r);

const int32_t *
pcm_resample_lsr_32(struct pcm_resample_state *state,
		    uint8_t channels,
		    unsigned src_rate,
		    const int32_t *src_buffer,
		    G_GNUC_UNUSED size_t src_size,
		    unsigned dest_rate, size_t *dest_size_r,
		    GError **error_r);

#endif

void
pcm_resample_fallback_deinit(struct pcm_resample_state *state);

const int16_t *
pcm_resample_fallback_16(struct pcm_resample_state *state,
			 uint8_t channels,
			 unsigned src_rate,
			 const int16_t *src_buffer, size_t src_size,
			 unsigned dest_rate,
			 size_t *dest_size_r);

const int32_t *
pcm_resample_fallback_32(struct pcm_resample_state *state,
			 uint8_t channels,
			 unsigned src_rate,
			 const int32_t *src_buffer,
			 G_GNUC_UNUSED size_t src_size,
			 unsigned dest_rate,
			 size_t *dest_size_r);

#endif
