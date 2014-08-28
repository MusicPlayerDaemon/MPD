/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "DetachedSong.hxx"

#include <assert.h>

DecoderControl::DecoderControl(Mutex &_mutex, Cond &_client_cond)
	:mutex(_mutex), client_cond(_client_cond),
	 state(DecoderState::STOP),
	 command(DecoderCommand::NONE),
	 client_is_waiting(false),
	 song(nullptr),
	 replay_gain_db(0), replay_gain_prev_db(0) {}

DecoderControl::~DecoderControl()
{
	ClearError();

	delete song;
}

void
DecoderControl::WaitForDecoder()
{
	assert(!client_is_waiting);
	client_is_waiting = true;

	client_cond.wait(mutex);

	assert(client_is_waiting);
	client_is_waiting = false;
}

bool
DecoderControl::IsCurrentSong(const DetachedSong &_song) const
{
	switch (state) {
	case DecoderState::STOP:
	case DecoderState::ERROR:
		return false;

	case DecoderState::START:
	case DecoderState::DECODE:
		return song->IsSame(_song);
	}

	assert(false);
	gcc_unreachable();
}

void
DecoderControl::Start(DetachedSong *_song,
		      SongTime _start_time, SongTime _end_time,
		      MusicBuffer &_buffer, MusicPipe &_pipe)
{
	assert(_song != nullptr);
	assert(_pipe.IsEmpty());

	delete song;
	song = _song;
	start_time = _start_time;
	end_time = _end_time;
	buffer = &_buffer;
	pipe = &_pipe;

	LockSynchronousCommand(DecoderCommand::START);
}

void
DecoderControl::Stop()
{
	Lock();

	if (command != DecoderCommand::NONE)
		/* Attempt to cancel the current command.  If it's too
		   late and the decoder thread is already executing
		   the old command, we'll call STOP again in this
		   function (see below). */
		SynchronousCommandLocked(DecoderCommand::STOP);

	if (state != DecoderState::STOP && state != DecoderState::ERROR)
		SynchronousCommandLocked(DecoderCommand::STOP);

	Unlock();
}

bool
DecoderControl::Seek(SongTime t)
{
	assert(state != DecoderState::START);

	if (state == DecoderState::STOP ||
	    state == DecoderState::ERROR || !seekable)
		return false;

	seek_time = t;
	seek_error = false;
	LockSynchronousCommand(DecoderCommand::SEEK);

	return !seek_error;
}

void
DecoderControl::Quit()
{
	assert(thread.IsDefined());

	quit = true;
	LockAsynchronousCommand(DecoderCommand::STOP);

	thread.Join();
}

void
DecoderControl::CycleMixRamp()
{
	previous_mix_ramp = std::move(mix_ramp);
	mix_ramp.Clear();
}
