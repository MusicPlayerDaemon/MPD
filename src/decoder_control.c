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
#include "decoder_control.h"
#include "player_control.h"

#include <assert.h>

void
dc_init(struct decoder_control *dc)
{
	dc->thread = NULL;

	dc->mutex = g_mutex_new();
	dc->cond = g_cond_new();

	dc->state = DECODE_STATE_STOP;
	dc->command = DECODE_COMMAND_NONE;
}

void
dc_deinit(struct decoder_control *dc)
{
	g_cond_free(dc->cond);
	g_mutex_free(dc->mutex);
}

static void
dc_command_wait_locked(struct decoder_control *dc)
{
	while (dc->command != DECODE_COMMAND_NONE)
		player_wait_decoder(dc);
}

void
dc_command_wait(struct decoder_control *dc)
{
	decoder_lock(dc);
	dc_command_wait_locked(dc);
	decoder_unlock(dc);
}

static void
dc_command_locked(struct decoder_control *dc, enum decoder_command cmd)
{
	dc->command = cmd;
	decoder_signal(dc);
	dc_command_wait_locked(dc);
}

static void
dc_command(struct decoder_control *dc, enum decoder_command cmd)
{
	decoder_lock(dc);
	dc_command_locked(dc, cmd);
	decoder_unlock(dc);
}

static void
dc_command_async(struct decoder_control *dc, enum decoder_command cmd)
{
	decoder_lock(dc);

	dc->command = cmd;
	decoder_signal(dc);

	decoder_unlock(dc);
}

void
dc_start(struct decoder_control *dc, struct song *song,
	 struct music_buffer *buffer, struct music_pipe *pipe)
{
	assert(song != NULL);
	assert(buffer != NULL);
	assert(pipe != NULL);

	dc->song = song;
	dc->buffer = buffer;
	dc->pipe = pipe;
	dc_command(dc, DECODE_COMMAND_START);
}

void
dc_stop(struct decoder_control *dc)
{
	decoder_lock(dc);

	if (dc->command != DECODE_COMMAND_NONE)
		/* Attempt to cancel the current command.  If it's too
		   late and the decoder thread is already executing
		   the old command, we'll call STOP again in this
		   function (see below). */
		dc_command_locked(dc, DECODE_COMMAND_STOP);

	if (dc->state != DECODE_STATE_STOP && dc->state != DECODE_STATE_ERROR)
		dc_command_locked(dc, DECODE_COMMAND_STOP);

	decoder_unlock(dc);
}

bool
dc_seek(struct decoder_control *dc, double where)
{
	assert(dc->state != DECODE_STATE_START);
	assert(where >= 0.0);

	if (dc->state == DECODE_STATE_STOP ||
	    dc->state == DECODE_STATE_ERROR || !dc->seekable)
		return false;

	dc->seek_where = where;
	dc->seek_error = false;
	dc_command(dc, DECODE_COMMAND_SEEK);

	if (dc->seek_error)
		return false;

	return true;
}

void
dc_quit(struct decoder_control *dc)
{
	assert(dc->thread != NULL);

	dc->quit = true;
	dc_command_async(dc, DECODE_COMMAND_STOP);

	g_thread_join(dc->thread);
	dc->thread = NULL;
}
