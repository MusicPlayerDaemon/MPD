/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
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

#ifndef MPD_PCM_RESAMPLE_H
#define MPD_PCM_RESAMPLE_H

#include "pcm_buffer.h"
#include "config.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef HAVE_LIBSAMPLERATE
#include <samplerate.h>
#endif

struct pcm_resample_state {
#ifdef HAVE_LIBSAMPLERATE
	SRC_STATE *state;
	SRC_DATA data;
	size_t data_in_size;
	size_t data_out_size;

	struct {
		unsigned src_rate;
		unsigned dest_rate;
		uint8_t channels;
	} prev;

	bool error;
#endif

	struct pcm_buffer buffer;
};

void pcm_resample_init(struct pcm_resample_state *state);

void pcm_resample_deinit(struct pcm_resample_state *state);

const int16_t *
pcm_resample_16(struct pcm_resample_state *state,
		uint8_t channels,
		unsigned src_rate,
		const int16_t *src_buffer, size_t src_size,
		unsigned dest_rate,
		size_t *dest_size_r);

const int32_t *
pcm_resample_24(struct pcm_resample_state *state,
		uint8_t channels,
		unsigned src_rate,
		const int32_t *src_buffer, size_t src_size,
		unsigned dest_rate,
		size_t *dest_size_r);

#endif
