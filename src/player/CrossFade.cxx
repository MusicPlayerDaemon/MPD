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

#include "CrossFade.hxx"
#include "Chrono.hxx"
#include "MusicChunk.hxx"
#include "pcm/AudioFormat.hxx"
#include "util/NumberParser.hxx"
#include "util/Domain.hxx"
#include "util/Math.hxx"
#include "Log.hxx"

#include <cassert>

static constexpr Domain cross_fade_domain("cross_fade");

inline bool
CrossFadeSettings::CanCrossFadeSong(SignedSongTime total_time) const noexcept
{
	return !total_time.IsNegative() &&
		total_time >= MIN_TOTAL_TIME &&
		duration < std::chrono::duration_cast<FloatDuration>(total_time);
}

gcc_pure
static FloatDuration
mixramp_interpolate(const char *ramp_list, float required_db) noexcept
{
	float last_db = 0;
	FloatDuration last_duration = FloatDuration::zero();
	bool have_last = false;

	/* ramp_list is a string of pairs of dBs and seconds that describe the
	 * volume profile. Delimiters are semi-colons between pairs and spaces
	 * between the dB and seconds of a pair.
	 * The dB values must be monotonically increasing for this to work. */

	while (true) {
		/* Parse the dB value. */
		char *endptr;
		const float db = ParseFloat(ramp_list, &endptr);
		if (endptr == ramp_list || *endptr != ' ')
			break;

		ramp_list = endptr + 1;

		/* Parse the time. */
		FloatDuration duration{ParseFloat(ramp_list, &endptr)};
		if (endptr == ramp_list || (*endptr != ';' && *endptr != 0))
			break;

		ramp_list = endptr;
		if (*ramp_list == ';')
			++ramp_list;

		/* Check for exact match. */
		if (db == required_db) {
			return duration;
		}

		/* Save if too quiet. */
		if (db < required_db) {
			last_db = db;
			last_duration = duration;
			have_last = true;
			continue;
		}

		/* If required db < any stored value, use the least. */
		if (!have_last)
			return duration;

		/* Finally, interpolate linearly. */
		duration = last_duration + (required_db - last_db) * (duration - last_duration) / (db - last_db);
		return duration;
	}

	return FloatDuration(-1);
}

bool
CrossFadeSettings::CanCrossFade(SignedSongTime current_total_time,
				SignedSongTime next_total_time,
				AudioFormat af,
				AudioFormat old_format) const noexcept
{
	return IsEnabled() &&
		CanCrossFadeSong(current_total_time) &&
		CanCrossFadeSong(next_total_time) &&
		/* we can't crossfade when the audio formats are different */
		af == old_format;
}

unsigned
CrossFadeSettings::Calculate(float replay_gain_db, float replay_gain_prev_db,
			     const char *mixramp_start, const char *mixramp_prev_end,
			     const AudioFormat af,
			     unsigned max_chunks) const noexcept
{
	assert(IsEnabled());

	unsigned int chunks = 0;

	assert(duration > FloatDuration::zero());
	assert(af.IsValid());

	const auto chunk_duration =
		af.SizeToTime<FloatDuration>(sizeof(MusicChunk::data));

	if (!IsMixRampEnabled() ||
	    !mixramp_start || !mixramp_prev_end) {
		chunks = lround(duration / chunk_duration);
	} else {
		/* Calculate mixramp overlap. */
		const auto mixramp_overlap_current =
			mixramp_interpolate(mixramp_start,
					    mixramp_db - replay_gain_db);
		const auto mixramp_overlap_prev =
			mixramp_interpolate(mixramp_prev_end,
					    mixramp_db - replay_gain_prev_db);
		const auto mixramp_overlap =
			mixramp_overlap_current + mixramp_overlap_prev;

		if (mixramp_overlap_current >= FloatDuration::zero() &&
		    mixramp_overlap_prev >= FloatDuration::zero() &&
		    mixramp_delay <= mixramp_overlap) {
			chunks = lround((mixramp_overlap - mixramp_delay)
					/ chunk_duration);
			FmtDebug(cross_fade_domain,
				 "will overlap {} chunks, {}s", chunks,
				 (mixramp_overlap - mixramp_delay).count());
		}
	}

	if (chunks > max_chunks) {
		chunks = max_chunks;
		LogWarning(cross_fade_domain,
			   "audio_buffer_size too small for computed MixRamp overlap");
	}

	return chunks;
}
