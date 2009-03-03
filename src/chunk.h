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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_CHUNK_H
#define MPD_CHUNK_H

#include <stdint.h>

enum {
	/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
	CHUNK_SIZE = 1020,
};

/**
 * A chunk of music data.  Its format is defined by the
 * music_pipe_append() caller.
 */
struct music_chunk {
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

	/** the data (probably PCM) */
	char data[CHUNK_SIZE];
};

void
music_chunk_init(struct music_chunk *chunk);

void
music_chunk_free(struct music_chunk *chunk);

#endif
