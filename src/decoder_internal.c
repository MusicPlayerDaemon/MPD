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

#include "config.h"
#include "decoder_internal.h"
#include "decoder_control.h"
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
need_chunks(struct decoder_control *dc, bool do_wait)
{
	if (dc->command == DECODE_COMMAND_STOP ||
	    dc->command == DECODE_COMMAND_SEEK)
		return dc->command;

	if (do_wait) {
		decoder_wait(dc);
		g_cond_signal(dc->client_cond);

		return dc->command;
	}

	return DECODE_COMMAND_NONE;
}

struct music_chunk *
decoder_get_chunk(struct decoder *decoder)
{
	struct decoder_control *dc = decoder->dc;
	enum decoder_command cmd;

	assert(decoder != NULL);

	if (decoder->chunk != NULL)
		return decoder->chunk;

	do {
		decoder->chunk = music_buffer_allocate(dc->buffer);
		if (decoder->chunk != NULL) {
			decoder->chunk->replay_gain_serial =
				decoder->replay_gain_serial;
			if (decoder->replay_gain_serial != 0)
				decoder->chunk->replay_gain_info =
					decoder->replay_gain_info;

			return decoder->chunk;
		}

		decoder_lock(dc);
		cmd = need_chunks(dc, true);
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
