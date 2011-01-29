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

#ifndef MPD_CROSSFADE_H
#define MPD_CROSSFADE_H

struct audio_format;
struct music_chunk;

/**
 * Calculate how many music pipe chunks should be used for crossfading.
 *
 * @param duration the requested crossfade duration
 * @param total_time total_time the duration of the new song
 * @param mixramp_db the current mixramp_db setting
 * @param mixramp_delay the current mixramp_delay setting
 * @param replay_gain_db the ReplayGain adjustment used for this song
 * @param replay_gain_prev_db the ReplayGain adjustment used on the last song
 * @param mixramp_start the next songs mixramp_start tag
 * @param mixramp_prev_end the last songs mixramp_end setting
 * @param af the audio format of the new song
 * @param old_format the audio format of the current song
 * @param max_chunks the maximum number of chunks
 * @return the number of chunks for crossfading, or 0 if cross fading
 * should be disabled for this song change
 */
unsigned cross_fade_calc(float duration, float total_time,
			 float mixramp_db, float mixramp_delay,
			 float replay_gain_db, float replay_gain_prev_db,
			 char *mixramp_start, char *mixramp_prev_end,
			 const struct audio_format *af,
			 const struct audio_format *old_format,
			 unsigned max_chunks);

#endif
