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

#ifndef MPD_DECODER_INTERNAL_H
#define MPD_DECODER_INTERNAL_H

#include "decoder_command.h"
#include "pcm_convert.h"

struct input_stream;

struct decoder {
	struct decoder_control *dc;

	struct pcm_convert_state conv_state;

	bool seeking;

	/**
	 * The tag from the song object.  This is only used for local
	 * files, because we expect the stream server to send us a new
	 * tag each time we play it.
	 */
	struct tag *song_tag;

	/** the last tag received from the stream */
	struct tag *stream_tag;

	/** the last tag received from the decoder plugin */
	struct tag *decoder_tag;

	/** the chunk currently being written to */
	struct music_chunk *chunk;
};

/**
 * Returns the current chunk the decoder writes to, or allocates a new
 * chunk if there is none.
 *
 * @return the chunk, or NULL if we have received a decoder command
 */
struct music_chunk *
decoder_get_chunk(struct decoder *decoder, struct input_stream *is);

/**
 * Flushes the current chunk.
 */
void
decoder_flush_chunk(struct decoder *decoder);

#endif
