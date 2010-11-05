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

#include "decoder_control.h"
#include "pipe.h"

#include <assert.h>

struct decoder_control dc;

void dc_init(void)
{
	notify_init(&dc.notify);
	dc.state = DECODE_STATE_STOP;
	dc.command = DECODE_COMMAND_NONE;
}

void dc_deinit(void)
{
	notify_deinit(&dc.notify);
}

void
dc_command_wait(struct notify *notify)
{
	while (dc.command != DECODE_COMMAND_NONE) {
		notify_signal(&dc.notify);
		notify_wait(notify);
	}
}

static void
dc_command(struct notify *notify, enum decoder_command cmd)
{
	dc.command = cmd;
	dc_command_wait(notify);
}

static void dc_command_async(enum decoder_command cmd)
{
	dc.command = cmd;
	notify_signal(&dc.notify);
}

void
dc_start(struct notify *notify, struct song *song, struct music_pipe *pipe)
{
	assert(dc.pipe == NULL);
	assert(song != NULL);
	assert(pipe != NULL);
	assert(music_pipe_empty(pipe));

	dc.next_song = song;
	dc.pipe = pipe;
	dc_command(notify, DECODE_COMMAND_START);
}

void
dc_start_async(struct song *song, struct music_pipe *pipe)
{
	assert(dc.pipe == NULL);
	assert(song != NULL);
	assert(pipe != NULL);
	assert(music_pipe_empty(pipe));

	dc.next_song = song;
	dc.pipe = pipe;
	dc_command_async(DECODE_COMMAND_START);
}

void
dc_stop(struct notify *notify)
{
	if (dc.command != DECODE_COMMAND_NONE)
		/* Attempt to cancel the current command.  If it's too
		   late and the decoder thread is already executing
		   the old command, we'll call STOP again in this
		   function (see below). */
		dc_command(notify, DECODE_COMMAND_STOP);

	if (dc.state != DECODE_STATE_STOP && dc.state != DECODE_STATE_ERROR)
		dc_command(notify, DECODE_COMMAND_STOP);
}

bool
dc_seek(struct notify *notify, double where)
{
	assert(dc.state != DECODE_STATE_START);
	assert(where >= 0.0);

	if (dc.state == DECODE_STATE_STOP ||
	    dc.state == DECODE_STATE_ERROR || !dc.seekable)
		return false;

	dc.seek_where = where;
	dc.seek_error = false;
	dc_command(notify, DECODE_COMMAND_SEEK);

	if (dc.seek_error)
		return false;

	return true;
}

void
dc_quit(void)
{
	assert(dc.thread != NULL);

	dc.quit = true;
	dc_command_async(DECODE_COMMAND_STOP);

	g_thread_join(dc.thread);
	dc.thread = NULL;
}
