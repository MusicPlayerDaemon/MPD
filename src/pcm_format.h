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

#ifndef PCM_FORMAT_H
#define PCM_FORMAT_H

#include <stdint.h>
#include <stddef.h>

struct pcm_convert_state;

const int16_t *
pcm_convert_to_16(struct pcm_convert_state *convert,
		  uint8_t bits, const void *src,
		  size_t src_size, size_t *dest_size_r);

const int32_t *
pcm_convert_to_24(uint8_t bits, const void *src,
		  size_t src_size, size_t *dest_size_r);

#endif
