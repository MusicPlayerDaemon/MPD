/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "CrossFade.hxx"
#include "Chrono.hxx"
#include "MusicChunk.hxx"
#include "AudioFormat.hxx"
#include "util/NumberParser.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>

static constexpr Domain cross_fade_domain("cross_fade");

gcc_pure
static float
mixramp_interpolate(const char *ramp_list, float required_db)
{
	float last_db = 0, last_secs = 0;
	bool have_last = false;

	/* ramp_list is a string of pairs of dBs and seconds that describe the
	 * volume profile. Delimiters are semi-colons between pairs and spaces
	 * between the dB and seconds of a pair.
	 * The dB values must be monotonically increasing for this to work. */

	while (1) {
		/* Parse the dB value. */
		char *endptr;
		const float db = ParseFloat(ramp_list, &endptr);
		if (endptr == ramp_list || *endptr != ' ')
			break;

		ramp_list = endptr + 1;

		/* Parse the time. */
		float secs = ParseFloat(ramp_list, &endptr);
		if (endptr == ramp_list || (*endptr != ';' && *endptr != 0))
			break;

		ramp_list = endptr;
		if (*ramp_list == ';')
			++ramp_list;

		/* Check for exact match. */
		if (db == required_db) {
			return secs;
		}

		/* Save if too quiet. */
		if (db < required_db) {
			last_db = db;
			last_secs = secs;
			have_last = true;
			continue;
		}

		/* If required db < any stored value, use the least. */
		if (!have_last)
			return secs;

		/* Finally, interpolate linearly. */
		secs = last_secs + (required_db - last_db) * (secs - last_secs) / (db - last_db);
		return secs;
	}

	return -1;
}

unsigned
CrossFadeSettings::Calculate(SignedSongTime total_time,
			     float replay_gain_db, float replay_gain_prev_db,
			     const char *mixramp_start, const char *mixramp_prev_end,
			     const AudioFormat af,
			     const AudioFormat old_format,
			     unsigned max_chunks) const
{
	unsigned int chunks = 0;
	float chunks_f;

	if (total_time.IsNegative() ||
	    duration < 0 || duration >= total_time.ToDoubleS() ||
	    /* we can't crossfade when the audio formats are different */
	    af != old_format)
		return 0;

	assert(duration >= 0);
	assert(af.IsValid());

	chunks_f = (float)af.GetTimeToSize() / (float)CHUNK_SIZE;

	if (mixramp_delay <= 0 || !mixramp_start || !mixramp_prev_end) {
		chunks = (chunks_f * duration + 0.5);
	} else {
		/* Calculate mixramp overlap. */
		const float mixramp_overlap_current =
			mixramp_interpolate(mixramp_start,
					    mixramp_db - replay_gain_db);
		const float mixramp_overlap_prev =
			mixramp_interpolate(mixramp_prev_end,
					    mixramp_db - replay_gain_prev_db);
		const float mixramp_overlap =
			mixramp_overlap_current + mixramp_overlap_prev;

		if (mixramp_overlap_current >= 0 &&
		    mixramp_overlap_prev >= 0 &&
		    mixramp_delay <= mixramp_overlap) {
			chunks = (chunks_f * (mixramp_overlap - mixramp_delay));
			FormatDebug(cross_fade_domain,
				    "will overlap %d chunks, %fs", chunks,
				    mixramp_overlap - mixramp_delay);
		}
	}

	if (chunks > max_chunks) {
		chunks = max_chunks;
		LogWarning(cross_fade_domain,
			   "audio_buffer_size too small for computed MixRamp overlap");
	}

	return chunks;
}
