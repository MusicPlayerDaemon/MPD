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

unsigned cross_fade_calc(float duration, float total_time,
			 const struct audio_format *af,
			 const struct audio_format *old_format,
			 unsigned max_chunks)
{
	unsigned int chunks;

	if (duration <= 0 || duration >= total_time ||
	    /* we can't crossfade when the audio formats are different */
	    !audio_format_equals(af, old_format))
		return 0;

	assert(duration > 0);
	assert(audio_format_valid(af));

	chunks = audio_format_time_to_size(af) / CHUNK_SIZE;
	chunks = (chunks * duration + 0.5);

	if (chunks > max_chunks)
		chunks = max_chunks;

	return chunks;
}

void cross_fade_apply(struct music_chunk *a, const struct music_chunk *b,
		      const struct audio_format *format,
		      unsigned int current_chunk, unsigned int num_chunks)
{
	size_t size;

	assert(a != NULL);
	assert(b != NULL);
	assert(a->length == 0 || b->length == 0 ||
	       audio_format_equals(&a->audio_format, &b->audio_format));
	assert(current_chunk <= num_chunks);

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
		((float)current_chunk) / num_chunks);

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
