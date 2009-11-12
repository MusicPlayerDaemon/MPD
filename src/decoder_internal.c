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
#include "decoder_internal.h"
#include "decoder_control.h"
#include "player_control.h"
#include "pipe.h"
#include "input_stream.h"
#include "buffer.h"
#include "chunk.h"

#include <assert.h>

/**
 * This is a wrapper for input_stream_buffer().  It assumes that the
 * decoder is currently locked, and temporarily unlocks it while
 * calling input_stream_buffer().  We shouldn't hold the lock during a
 * potentially blocking operation.
 */
static int
decoder_input_buffer(struct decoder_control *dc, struct input_stream *is)
{
	int ret;

	decoder_unlock(dc);
	ret = input_stream_buffer(is) > 0;
	decoder_lock(dc);

	return ret;
}

/**
 * All chunks are full of decoded data; wait for the player to free
 * one.
 */
static enum decoder_command
need_chunks(struct decoder_control *dc, struct input_stream *is, bool do_wait)
{
	if (dc->command == DECODE_COMMAND_STOP ||
	    dc->command == DECODE_COMMAND_SEEK)
		return dc->command;

	if ((is == NULL || decoder_input_buffer(dc, is) <= 0) && do_wait) {
		decoder_wait(dc);
		player_signal();

		return dc->command;
	}

	return DECODE_COMMAND_NONE;
}

struct music_chunk *
decoder_get_chunk(struct decoder *decoder, struct input_stream *is)
{
	struct decoder_control *dc = decoder->dc;
	enum decoder_command cmd;

	assert(decoder != NULL);

	if (decoder->chunk != NULL)
		return decoder->chunk;

	do {
		decoder->chunk = music_buffer_allocate(dc->buffer);
		if (decoder->chunk != NULL)
			return decoder->chunk;

		decoder_lock(dc);
		cmd = need_chunks(dc, is, true);
		decoder_unlock(dc);
	} while (cmd == DECODE_COMMAND_NONE);

	return NULL;
}

void
decoder_flush_chunk(struct decoder *decoder)
{
	struct decoder_control *dc = decoder->dc;

	assert(decoder != NULL);
	assert(decoder->chunk != NULL);

	if (music_chunk_is_empty(decoder->chunk))
		music_buffer_return(dc->buffer, decoder->chunk);
	else
		music_pipe_push(dc->pipe, decoder->chunk);

	decoder->chunk = NULL;
}
