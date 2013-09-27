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
#include "PlayerControl.hxx"
#include "Idle.hxx"
#include "Song.hxx"
#include "DecoderControl.hxx"

#include <cmath>

#include <assert.h>

player_control::player_control(unsigned _buffer_chunks,
			       unsigned _buffered_before_play)
	:buffer_chunks(_buffer_chunks),
	 buffered_before_play(_buffered_before_play),
	 thread(nullptr),
	 command(PLAYER_COMMAND_NONE),
	 state(PLAYER_STATE_STOP),
	 error_type(PLAYER_ERROR_NONE),
	 next_song(nullptr),
	 cross_fade_seconds(0),
	 mixramp_db(0),
#if defined(__GLIBCXX__) && !defined(_GLIBCXX_USE_C99_MATH_TR1)
	 /* workaround: on MacPorts, this option is disabled on gcc47,
	    and therefore std::nanf() is not available */
	 mixramp_delay_seconds(nanf("")),
#else
	 mixramp_delay_seconds(std::nanf("")),
#endif
	 total_play_time(0),
	 border_pause(false)
{
}

player_control::~player_control()
{
	if (next_song != nullptr)
		next_song->Free();
}

void
player_control::Play(Song *song)
{
	assert(song != NULL);

	Lock();

	if (state != PLAYER_STATE_STOP)
		SynchronousCommand(PLAYER_COMMAND_STOP);

	assert(next_song == nullptr);

	EnqueueSongLocked(song);

	assert(next_song == nullptr);

	Unlock();
}

void
player_control::Cancel()
{
	LockSynchronousCommand(PLAYER_COMMAND_CANCEL);
	assert(next_song == NULL);
}

void
player_control::Stop()
{
	LockSynchronousCommand(PLAYER_COMMAND_CLOSE_AUDIO);
	assert(next_song == nullptr);

	idle_add(IDLE_PLAYER);
}

void
player_control::UpdateAudio()
{
	LockSynchronousCommand(PLAYER_COMMAND_UPDATE_AUDIO);
}

void
player_control::Kill()
{
	assert(thread != NULL);

	LockSynchronousCommand(PLAYER_COMMAND_EXIT);
	g_thread_join(thread);
	thread = NULL;

	idle_add(IDLE_PLAYER);
}

void
player_control::PauseLocked()
{
	if (state != PLAYER_STATE_STOP) {
		SynchronousCommand(PLAYER_COMMAND_PAUSE);
		idle_add(IDLE_PLAYER);
	}
}

void
player_control::Pause()
{
	Lock();
	PauseLocked();
	Unlock();
}

void
player_control::SetPause(bool pause_flag)
{
	Lock();

	switch (state) {
	case PLAYER_STATE_STOP:
		break;

	case PLAYER_STATE_PLAY:
		if (pause_flag)
			PauseLocked();
		break;

	case PLAYER_STATE_PAUSE:
		if (!pause_flag)
			PauseLocked();
		break;
	}

	Unlock();
}

void
player_control::SetBorderPause(bool _border_pause)
{
	Lock();
	border_pause = _border_pause;
	Unlock();
}

player_status
player_control::GetStatus()
{
	player_status status;

	Lock();
	SynchronousCommand(PLAYER_COMMAND_REFRESH);

	status.state = state;

	if (state != PLAYER_STATE_STOP) {
		status.bit_rate = bit_rate;
		status.audio_format = audio_format;
		status.total_time = total_time;
		status.elapsed_time = elapsed_time;
	}

	Unlock();

	return status;
}

void
player_control::SetError(player_error type, Error &&_error)
{
	assert(type != PLAYER_ERROR_NONE);
	assert(_error.IsDefined());

	error_type = type;
	error = std::move(_error);
}

void
player_control::ClearError()
{
	Lock();

	if (error_type != PLAYER_ERROR_NONE) {
	    error_type = PLAYER_ERROR_NONE;
	    error.Clear();
	}

	Unlock();
}

char *
player_control::GetErrorMessage() const
{
	Lock();
	char *message = error_type != PLAYER_ERROR_NONE
		? g_strdup(error.GetMessage())
		: NULL;
	Unlock();
	return message;
}

void
player_control::EnqueueSong(Song *song)
{
	assert(song != NULL);

	Lock();
	EnqueueSongLocked(song);
	Unlock();
}

bool
player_control::Seek(Song *song, float seek_time)
{
	assert(song != NULL);

	Lock();

	if (next_song != nullptr)
		next_song->Free();

	next_song = song;
	seek_where = seek_time;
	SynchronousCommand(PLAYER_COMMAND_SEEK);
	Unlock();

	assert(next_song == nullptr);

	idle_add(IDLE_PLAYER);

	return true;
}

void
player_control::SetCrossFade(float _cross_fade_seconds)
{
	if (_cross_fade_seconds < 0)
		_cross_fade_seconds = 0;
	cross_fade_seconds = _cross_fade_seconds;

	idle_add(IDLE_OPTIONS);
}

void
player_control::SetMixRampDb(float _mixramp_db)
{
	mixramp_db = _mixramp_db;

	idle_add(IDLE_OPTIONS);
}

void
player_control::SetMixRampDelay(float _mixramp_delay_seconds)
{
	mixramp_delay_seconds = _mixramp_delay_seconds;

	idle_add(IDLE_OPTIONS);
}
