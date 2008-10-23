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

#include "pcm_resample.h"
#include "gcc.h"

/* resampling code blatantly ripped from ESD */
size_t
pcm_resample_16(uint8_t channels,
		unsigned src_rate,
		const int16_t *src_buffer, mpd_unused size_t src_size,
		unsigned dest_rate,
		int16_t *dest_buffer, size_t dest_size,
		mpd_unused struct pcm_resample_state *state)
{
	unsigned src_pos, dest_pos = 0;
	unsigned dest_samples = dest_size / 2;
	int16_t lsample, rsample;

	switch (channels) {
	case 1:
		while (dest_pos < dest_samples) {
			src_pos = dest_pos * src_rate / dest_rate;

			lsample = src_buffer[src_pos++];

			dest_buffer[dest_pos++] = lsample;
		}
		break;
	case 2:
		while (dest_pos < dest_samples) {
			src_pos = dest_pos * src_rate / dest_rate;
			src_pos &= ~1;

			lsample = src_buffer[src_pos++];
			rsample = src_buffer[src_pos++];

			dest_buffer[dest_pos++] = lsample;
			dest_buffer[dest_pos++] = rsample;
		}
		break;
	}

	return dest_size;
}
