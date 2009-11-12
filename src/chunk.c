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
#include "chunk.h"
#include "audio_format.h"
#include "tag.h"

#include <assert.h>

void
music_chunk_init(struct music_chunk *chunk)
{
	chunk->length = 0;
	chunk->tag = NULL;
}

void
music_chunk_free(struct music_chunk *chunk)
{
	if (chunk->tag != NULL)
		tag_free(chunk->tag);
}

#ifndef NDEBUG
bool
music_chunk_check_format(const struct music_chunk *chunk,
			 const struct audio_format *audio_format)
{
	assert(chunk != NULL);
	assert(audio_format != NULL);
	assert(audio_format_valid(audio_format));

	return chunk->length == 0 ||
		audio_format_equals(&chunk->audio_format, audio_format);
}
#endif

void *
music_chunk_write(struct music_chunk *chunk,
		  const struct audio_format *audio_format,
		  float data_time, uint16_t bit_rate,
		  size_t *max_length_r)
{
	const size_t frame_size = audio_format_frame_size(audio_format);
	size_t num_frames;

	assert(music_chunk_check_format(chunk, audio_format));
	assert(chunk->length == 0 || audio_format_valid(&chunk->audio_format));

	if (chunk->length == 0) {
		/* if the chunk is empty, nobody has set bitRate and
		   times yet */

		chunk->bit_rate = bit_rate;
		chunk->times = data_time;
	}

	num_frames = (sizeof(chunk->data) - chunk->length) / frame_size;
	if (num_frames == 0)
		return NULL;

#ifndef NDEBUG
	chunk->audio_format = *audio_format;
#endif

	*max_length_r = num_frames * frame_size;
	return chunk->data + chunk->length;
}

bool
music_chunk_expand(struct music_chunk *chunk,
		   const struct audio_format *audio_format, size_t length)
{
	const size_t frame_size = audio_format_frame_size(audio_format);

	assert(chunk != NULL);
	assert(chunk->length + length <= sizeof(chunk->data));
	assert(audio_format_equals(&chunk->audio_format, audio_format));

	chunk->length += length;

	return chunk->length + frame_size > sizeof(chunk->data);
}
