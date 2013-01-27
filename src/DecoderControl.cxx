/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DecoderControl.hxx"
#include "MusicPipe.hxx"
#include "song.h"

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "decoder_control"

decoder_control::decoder_control()
	:thread(nullptr),
	 state(DECODE_STATE_STOP),
	 command(DECODE_COMMAND_NONE),
	 song(nullptr),
	 replay_gain_db(0), replay_gain_prev_db(0),
	 mixramp_start(nullptr), mixramp_end(nullptr),
	 mixramp_prev_end(nullptr) {}

decoder_control::~decoder_control()
{
	ClearError();

	if (song != NULL)
		song_free(song);

	g_free(mixramp_start);
	g_free(mixramp_end);
	g_free(mixramp_prev_end);
}

static void
dc_command_wait_locked(struct decoder_control *dc)
{
	while (dc->command != DECODE_COMMAND_NONE)
		dc->WaitForDecoder();
}

static void
dc_command_locked(struct decoder_control *dc, enum decoder_command cmd)
{
	dc->command = cmd;
	dc->Signal();
	dc_command_wait_locked(dc);
}

static void
dc_command(struct decoder_control *dc, enum decoder_command cmd)
{
	dc->Lock();
	dc->ClearError();
	dc_command_locked(dc, cmd);
	dc->Unlock();
}

static void
dc_command_async(struct decoder_control *dc, enum decoder_command cmd)
{
	dc->Lock();

	dc->command = cmd;
	dc->Signal();

	dc->Unlock();
}

bool
decoder_control::IsCurrentSong(const struct song *_song) const
{
	assert(_song != NULL);

	switch (state) {
	case DECODE_STATE_STOP:
	case DECODE_STATE_ERROR:
		return false;

	case DECODE_STATE_START:
	case DECODE_STATE_DECODE:
		return song_equals(song, _song);
	}

	assert(false);
	return false;
}

void
decoder_control::Start(struct song *_song,
		       unsigned _start_ms, unsigned _end_ms,
		       music_buffer *_buffer, music_pipe *_pipe)
{
	assert(_song != NULL);
	assert(_buffer != NULL);
	assert(_pipe != NULL);
	assert(music_pipe_empty(_pipe));

	if (song != nullptr)
		song_free(song);

	song = _song;
	start_ms = _start_ms;
	end_ms = _end_ms;
	buffer = _buffer;
	pipe = _pipe;

	dc_command(this, DECODE_COMMAND_START);
}

void
decoder_control::Stop()
{
	Lock();

	if (command != DECODE_COMMAND_NONE)
		/* Attempt to cancel the current command.  If it's too
		   late and the decoder thread is already executing
		   the old command, we'll call STOP again in this
		   function (see below). */
		dc_command_locked(this, DECODE_COMMAND_STOP);

	if (state != DECODE_STATE_STOP && state != DECODE_STATE_ERROR)
		dc_command_locked(this, DECODE_COMMAND_STOP);

	Unlock();
}

bool
decoder_control::Seek(double where)
{
	assert(state != DECODE_STATE_START);
	assert(where >= 0.0);

	if (state == DECODE_STATE_STOP ||
	    state == DECODE_STATE_ERROR || !seekable)
		return false;

	seek_where = where;
	seek_error = false;
	dc_command(this, DECODE_COMMAND_SEEK);

	return !seek_error;
}

void
decoder_control::Quit()
{
	assert(thread != nullptr);

	quit = true;
	dc_command_async(this, DECODE_COMMAND_STOP);

	g_thread_join(thread);
	thread = nullptr;
}

void
decoder_control::MixRampStart(char *_mixramp_start)
{
	g_free(mixramp_start);
	mixramp_start = _mixramp_start;
}

void
decoder_control::MixRampEnd(char *_mixramp_end)
{
	g_free(mixramp_end);
	mixramp_end = _mixramp_end;
}

void
decoder_control::MixRampPrevEnd(char *_mixramp_prev_end)
{
	g_free(mixramp_prev_end);
	mixramp_prev_end = _mixramp_prev_end;
}
