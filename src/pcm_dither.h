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

#ifndef MPD_PCM_DITHER_H
#define MPD_PCM_DITHER_H

#include <stdint.h>

struct pcm_dither {
	int32_t error[3];
	int32_t random;
};

static inline void
pcm_dither_24_init(struct pcm_dither *dither)
{
	dither->error[0] = dither->error[1] = dither->error[2] = 0;
	dither->random = 0;
}

void
pcm_dither_24_to_16(struct pcm_dither *dither,
		    int16_t *dest, const int32_t *src, const int32_t *src_end);

void
pcm_dither_32_to_16(struct pcm_dither *dither,
		    int16_t *dest, const int32_t *src, const int32_t *src_end);

#endif
