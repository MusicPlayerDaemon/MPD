/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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

#ifndef MPD_CROSSFADE_H
#define MPD_CROSSFADE_H

struct audio_format;
struct music_chunk;

/**
 * Calculate how many music pipe chunks should be used for crossfading.
 *
 * @param duration the requested crossfade duration
 * @param total_time total_time the duration of the new song
 * @param af the audio format of the new song
 * @param old_format the audio format of the current song
 * @param max_chunks the maximum number of chunks
 * @return the number of chunks for crossfading, or 0 if cross fading
 * should be disabled for this song change
 */
unsigned cross_fade_calc(float duration, float total_time,
			 const struct audio_format *af,
			 const struct audio_format *old_format,
			 unsigned max_chunks);

/**
 * Applies cross fading to two chunks, i.e. mixes these chunks.
 * Internally, this calls pcm_mix().
 *
 * @param a the chunk in the current song (and the destination chunk)
 * @param b the according chunk in the new song
 * @param format the audio format of both chunks (must be the same)
 * @param current_chunk the relative index of the current chunk
 * @param num_chunks the number of chunks used for cross fading
 */
void cross_fade_apply(struct music_chunk *a, const struct music_chunk *b,
		      const struct audio_format *format,
		      unsigned int current_chunk, unsigned int num_chunks);

#endif
