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

#include "config.h"
#include "crossfade.h"
#include "pcm_mix.h"
#include "chunk.h"
#include "audio_format.h"
#include "tag.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "crossfade"

static float mixramp_interpolate(char *ramp_list, float required_db)
{
	float db, secs, last_db = nan(""), last_secs = 0;
	char *ramp_str, *save_str = NULL;

	/* ramp_list is a string of pairs of dBs and seconds that describe the
	 * volume profile. Delimiters are semi-colons between pairs and spaces
	 * between the dB and seconds of a pair.
	 * The dB values must be monotonically increasing for this to work. */

	while (1) {
		/* Parse the dB tokens out of the input string. */
		ramp_str = strtok_r(ramp_list, " ", &save_str);

		/* Tell strtok to continue next time round. */
		ramp_list = NULL;

		/* Parse the dB value. */
		if (NULL == ramp_str) {
			return nan("");
		}
		db = (float)atof(ramp_str);

		/* Parse the time. */
		ramp_str = strtok_r(NULL, ";", &save_str);
		if (NULL == ramp_str) {
			return nan("");
		}
		secs = (float)atof(ramp_str);

		/* Check for exact match. */
		if (db == required_db) {
			return secs;
		}

		/* Save if too quiet. */
		if (db < required_db) {
			last_db = db;
			last_secs = secs;
			continue;
		}

		/* If required db < any stored value, use the least. */
		if (isnan(last_db)) {
			return secs;
		}

		/* Finally, interpolate linearly. */
		secs = last_secs + (required_db - last_db) * (secs - last_secs) / (db - last_db);
		return secs;
	}
}

unsigned cross_fade_calc(float duration, float total_time,
			 float mixramp_db, float mixramp_delay,
			 char *mixramp_start, char *mixramp_prev_end,
			 const struct audio_format *af,
			 const struct audio_format *old_format,
			 unsigned max_chunks)
{
	unsigned int chunks = 0;
	float chunks_f;
	float mixramp_overlap;

	if (duration < 0 || duration >= total_time ||
	    /* we can't crossfade when the audio formats are different */
	    !audio_format_equals(af, old_format))
		return 0;

	assert(duration >= 0);
	assert(audio_format_valid(af));

	chunks_f = (float)audio_format_time_to_size(af) / (float)CHUNK_SIZE;

	if (isnan(mixramp_delay) || !(mixramp_start) || !(mixramp_prev_end)) {
		chunks = (chunks_f * duration + 0.5);
	} else {
		/* Calculate mixramp overlap.
		 * FIXME factor in ReplayGain for both songs. */
		mixramp_overlap = mixramp_interpolate(mixramp_start, mixramp_db)
		  + mixramp_interpolate(mixramp_prev_end, mixramp_db);
		if (!isnan(mixramp_overlap) && (mixramp_delay <= mixramp_overlap)) {
			chunks = (chunks_f * (mixramp_overlap - mixramp_delay));
			g_debug("will overlap %d chunks, %fs", chunks,
				mixramp_overlap - mixramp_delay);
		}
	}

	if (chunks > max_chunks) {
		chunks = max_chunks;
		g_warning("audio_buffer_size too small for computed MixRamp overlap");
	}

	return chunks;
}

void cross_fade_apply(struct music_chunk *a, const struct music_chunk *b,
		      const struct audio_format *format,
		      float mix_ratio)
{
	size_t size;

	assert(a != NULL);
	assert(b != NULL);
	assert(a->length == 0 || b->length == 0 ||
	       audio_format_equals(&a->audio_format, &b->audio_format));

	if (a->tag == NULL && b->tag != NULL)
		/* merge the tag into the destination chunk */
		a->tag = tag_dup(b->tag);

	size = b->length > a->length
		? a->length
		: b->length;

	pcm_mix(a->data,
		b->data,
		size,
		format,
		mix_ratio);

	if (b->length > a->length) {
		/* the second buffer is larger than the first one:
		   there is unmixed rest at the end.  Copy it over.
		   The output buffer API guarantees that there is
		   enough room in a->data. */

#ifndef NDEBUG
		if (a->length == 0)
			a->audio_format = b->audio_format;
#endif

		memcpy(a->data + a->length,
		       b->data + a->length,
		       b->length - a->length);
		a->length = b->length;
	}
}
