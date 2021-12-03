/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_CROSSFADE_HXX
#define MPD_CROSSFADE_HXX

#include "Chrono.hxx"

struct AudioFormat;
class SignedSongTime;

struct CrossFadeSettings {
	/**
	 * Songs shorter than this will never cross-fade.
	 */
	static constexpr SignedSongTime MIN_TOTAL_TIME{std::chrono::seconds{20}};

	/**
	 * The configured cross fade duration [s].
	 */
	FloatDuration duration{0};

	float mixramp_db{0};

	/**
	 * The configured MixRapm delay [s].  A non-positive value
	 * disables MixRamp.
	 */
	FloatDuration mixramp_delay{-1};

	constexpr bool IsEnabled() const noexcept {
		return duration.count() > 0;
	}

	constexpr bool IsMixRampEnabled() const noexcept {
		return mixramp_delay > FloatDuration::zero();
	}

	/**
	 * Determine whether cross-fading the two songs is possible.
	 *
	 * @param current_total_time the duration of the current song
	 * @param next_total_time the duration of the new song
	 * @param af the audio format of the new song
	 * @param old_format the audio format of the current song
	 * @return true if cross-fading is possible
	 */
	[[gnu::pure]]
	bool CanCrossFade(SignedSongTime current_total_time,
			  SignedSongTime next_total_time,
			  AudioFormat af, AudioFormat old_format) const noexcept;

	/**
	 * Calculate how many music pipe chunks should be used for crossfading.
	 *
	 * @param replay_gain_db the ReplayGain adjustment used for this song
	 * @param replay_gain_prev_db the ReplayGain adjustment used on the last song
	 * @param mixramp_start the next songs mixramp_start tag
	 * @param mixramp_prev_end the last songs mixramp_end setting
	 * @param af the audio format of the new song
	 * @param max_chunks the maximum number of chunks
	 * @return the number of chunks for crossfading, or 0 if cross fading
	 * should be disabled for this song change
	 */
	[[gnu::pure]]
	unsigned Calculate(float replay_gain_db, float replay_gain_prev_db,
			   const char *mixramp_start,
			   const char *mixramp_prev_end,
			   AudioFormat af,
			   unsigned max_chunks) const noexcept;

private:
	/**
	 * Can the described song be cross-faded?
	 */
	[[gnu::pure]]
	bool CanCrossFadeSong(SignedSongTime total_time) const noexcept;
};

#endif
