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

#include "decoder_internal.h"
#include "decoder_control.h"
#include "player_control.h"
#include "pipe.h"
#include "input_stream.h"
#include "buffer.h"
#include "chunk.h"

#include <assert.h>

/**
 * All chunks are full of decoded data; wait for the player to free
 * one.
 */
static enum decoder_command
need_chunks(struct input_stream *is, bool do_wait)
{
	if (dc.command == DECODE_COMMAND_STOP ||
	    dc.command == DECODE_COMMAND_SEEK)
		return dc.command;

	if ((is == NULL || input_stream_buffer(is) <= 0) && do_wait) {
		notify_wait(&dc.notify);
		notify_signal(&pc.notify);

		return dc.command;
	}

	return DECODE_COMMAND_NONE;
}

struct music_chunk *
decoder_get_chunk(struct decoder *decoder, struct input_stream *is)
{
	enum decoder_command cmd;

	assert(decoder != NULL);

	if (decoder->chunk != NULL)
		return decoder->chunk;

	do {
		decoder->chunk = music_buffer_allocate(dc.buffer);
		if (decoder->chunk != NULL)
			return decoder->chunk;

		cmd = need_chunks(is, true);
	} while (cmd == DECODE_COMMAND_NONE);

	return NULL;
}

void
decoder_flush_chunk(struct decoder *decoder)
{
	assert(decoder != NULL);
	assert(decoder->chunk != NULL);

	if (music_chunk_is_empty(decoder->chunk))
		music_buffer_return(dc.buffer, decoder->chunk);
	else
		music_pipe_push(dc.pipe, decoder->chunk);

	decoder->chunk = NULL;
}
