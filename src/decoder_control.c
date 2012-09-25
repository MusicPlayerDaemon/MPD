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
#include "decoder_control.h"
#include "pipe.h"

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "decoder_control"

struct decoder_control *
dc_new(GCond *client_cond)
{
	struct decoder_control *dc = g_new(struct decoder_control, 1);

	dc->thread = NULL;

	dc->mutex = g_mutex_new();
	dc->cond = g_cond_new();
	dc->client_cond = client_cond;

	dc->state = DECODE_STATE_STOP;
	dc->command = DECODE_COMMAND_NONE;

	dc->replay_gain_db = 0;
	dc->replay_gain_prev_db = 0;
	dc->mixramp_start = NULL;
	dc->mixramp_end = NULL;
	dc->mixramp_prev_end = NULL;

	return dc;
}

void
dc_free(struct decoder_control *dc)
{
	g_cond_free(dc->cond);
	g_mutex_free(dc->mutex);
	g_free(dc->mixramp_start);
	g_free(dc->mixramp_end);
	g_free(dc->mixramp_prev_end);
	g_free(dc);
}

static void
dc_command_wait_locked(struct decoder_control *dc)
{
	while (dc->command != DECODE_COMMAND_NONE)
		g_cond_wait(dc->client_cond, dc->mutex);
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
	 unsigned start_ms, unsigned end_ms,
	 struct music_buffer *buffer, struct music_pipe *pipe)
{
	assert(song != NULL);
	assert(buffer != NULL);
	assert(pipe != NULL);
	assert(music_pipe_empty(pipe));

	dc->song = song;
	dc->start_ms = start_ms;
	dc->end_ms = end_ms;
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

void
dc_mixramp_start(struct decoder_control *dc, char *mixramp_start)
{
	assert(dc != NULL);

	g_free(dc->mixramp_start);
	dc->mixramp_start = mixramp_start;
}

void
dc_mixramp_end(struct decoder_control *dc, char *mixramp_end)
{
	assert(dc != NULL);

	g_free(dc->mixramp_end);
	dc->mixramp_end = mixramp_end;
}

void
dc_mixramp_prev_end(struct decoder_control *dc, char *mixramp_prev_end)
{
	assert(dc != NULL);

	g_free(dc->mixramp_prev_end);
	dc->mixramp_prev_end = mixramp_prev_end;
}
