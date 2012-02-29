/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#ifndef MPD_PCM_DSD_H
#define MPD_PCM_DSD_H

#include "check.h"
#include "pcm_buffer.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * Wrapper for the dsd2pcm library.
 */
struct pcm_dsd {
	struct pcm_buffer buffer;

	struct dsd2pcm_ctx_s *dsd2pcm[32];
};

void
pcm_dsd_init(struct pcm_dsd *dsd);

void
pcm_dsd_deinit(struct pcm_dsd *dsd);

void
pcm_dsd_reset(struct pcm_dsd *dsd);

const float *
pcm_dsd_to_float(struct pcm_dsd *dsd, unsigned channels, bool lsbfirst,
		 const uint8_t *src, size_t src_size,
		 size_t *dest_size_r);

#endif
