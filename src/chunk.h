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

#ifndef MPD_CHUNK_H
#define MPD_CHUNK_H

#include "replay_gain_info.h"

#ifndef NDEBUG
#include "audio_format.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

enum {
	CHUNK_SIZE = 4096,
};

struct audio_format;

/**
 * A chunk of music data.  Its format is defined by the
 * music_pipe_append() caller.
 */
struct music_chunk {
	/** the next chunk in a linked list */
	struct music_chunk *next;

	/**
	 * An optional chunk which should be mixed into this chunk.
	 * This is used for cross-fading.
	 */
	struct music_chunk *other;

	/**
	 * The current mix ratio for cross-fading: 1.0 means play 100%
	 * of this chunk, 0.0 means play 100% of the "other" chunk.
	 */
	float mix_ratio;

	/** number of bytes stored in this chunk */
	uint16_t length;

	/** current bit rate of the source file */
	uint16_t bit_rate;

	/** the time stamp within the song */
	float times;

	/**
	 * An optional tag associated with this chunk (and the
	 * following chunks); appears at song boundaries.  The tag
	 * object is owned by this chunk, and must be freed when this
	 * chunk is deinitialized in music_chunk_free()
	 */
	struct tag *tag;

	/**
	 * Replay gain information associated with this chunk.
	 * Only valid if the serial is not 0.
	 */
	struct replay_gain_info replay_gain_info;

	/**
	 * A serial number for checking if replay gain info has
	 * changed since the last chunk.  The magic value 0 indicates
	 * that there is no replay gain info available.
	 */
	unsigned replay_gain_serial;

	/** the data (probably PCM) */
	char data[CHUNK_SIZE];

#ifndef NDEBUG
	struct audio_format audio_format;
#endif
};

void
music_chunk_init(struct music_chunk *chunk);

void
music_chunk_free(struct music_chunk *chunk);

static inline bool
music_chunk_is_empty(const struct music_chunk *chunk)
{
	return chunk->length == 0 && chunk->tag == NULL;
}

#ifndef NDEBUG
/**
 * Checks if the audio format if the chunk is equal to the specified
 * audio_format.
 */
bool
music_chunk_check_format(const struct music_chunk *chunk,
			 const struct audio_format *audio_format);
#endif

/**
 * Prepares appending to the music chunk.  Returns a buffer where you
 * may write into.  After you are finished, call music_chunk_expand().
 *
 * @param chunk the music_chunk object
 * @param audio_format the audio format for the appended data; must
 * stay the same for the life cycle of this chunk
 * @param data_time the time within the song
 * @param bit_rate the current bit rate of the source file
 * @param max_length_r the maximum write length is returned here
 * @return a writable buffer, or NULL if the chunk is full
 */
void *
music_chunk_write(struct music_chunk *chunk,
		  const struct audio_format *audio_format,
		  float data_time, uint16_t bit_rate,
		  size_t *max_length_r);

/**
 * Increases the length of the chunk after the caller has written to
 * the buffer returned by music_chunk_write().
 *
 * @param chunk the music_chunk object
 * @param audio_format the audio format for the appended data; must
 * stay the same for the life cycle of this chunk
 * @param length the number of bytes which were appended
 * @return true if the chunk is full
 */
bool
music_chunk_expand(struct music_chunk *chunk,
		   const struct audio_format *audio_format, size_t length);

#endif
