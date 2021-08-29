/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Control.hxx"
#include "MusicPipe.hxx"
#include "song/DetachedSong.hxx"

#include <cassert>
#include <stdexcept>

DecoderControl::DecoderControl(Mutex &_mutex, Cond &_client_cond,
			       InputCacheManager *_input_cache,
			       const AudioFormat _configured_audio_format,
			       const ReplayGainConfig &_replay_gain_config) noexcept
	:thread(BIND_THIS_METHOD(RunThread)),
	 input_cache(_input_cache),
	 mutex(_mutex), client_cond(_client_cond),
	 configured_audio_format(_configured_audio_format),
	 replay_gain_config(_replay_gain_config) {}

DecoderControl::~DecoderControl() noexcept
{
	ClearError();
}

void
DecoderControl::SetReady(const AudioFormat audio_format,
			 bool _seekable, SignedSongTime _duration) noexcept
{
	assert(state == DecoderState::START);
	assert(pipe != nullptr);
	assert(pipe->IsEmpty());
	assert(audio_format.IsDefined());
	assert(audio_format.IsValid());

	in_audio_format = audio_format;
	out_audio_format = audio_format.WithMask(configured_audio_format);

	seekable = _seekable;
	total_time = _duration;

	state = DecoderState::DECODE;
	client_cond.notify_one();
}

bool
DecoderControl::IsCurrentSong(const DetachedSong &_song) const noexcept
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
DecoderControl::Start(std::unique_lock<Mutex> &lock,
		      std::unique_ptr<DetachedSong> _song,
		      SongTime _start_time, SongTime _end_time,
		      bool _initial_seek_essential,
		      MusicBuffer &_buffer,
		      std::shared_ptr<MusicPipe> _pipe) noexcept
{
	assert(_song != nullptr);
	assert(_pipe->IsEmpty());

	song = std::move(_song);
	start_time = _start_time;
	end_time = _end_time;
	initial_seek_essential = _initial_seek_essential;
	buffer = &_buffer;
	pipe = std::move(_pipe);

	ClearError();
	SynchronousCommandLocked(lock, DecoderCommand::START);
}

void
DecoderControl::Stop(std::unique_lock<Mutex> &lock) noexcept
{
	if (command != DecoderCommand::NONE)
		/* Attempt to cancel the current command.  If it's too
		   late and the decoder thread is already executing
		   the old command, we'll call STOP again in this
		   function (see below). */
		SynchronousCommandLocked(lock, DecoderCommand::STOP);

	if (state != DecoderState::STOP && state != DecoderState::ERROR)
		SynchronousCommandLocked(lock, DecoderCommand::STOP);
}

void
DecoderControl::Seek(std::unique_lock<Mutex> &lock, SongTime t)
{
	assert(state != DecoderState::START);
	assert(state != DecoderState::ERROR);

	switch (state) {
	case DecoderState::START:
	case DecoderState::ERROR:
		gcc_unreachable();

	case DecoderState::STOP:
		/* TODO: if this happens, the caller should be given a
		   chance to restart the decoder */
		throw std::runtime_error("Decoder is dead");

	case DecoderState::DECODE:
		break;
	}

	if (!seekable)
		throw std::runtime_error("Not seekable");

	seek_time = t;
	seek_error = false;
	SynchronousCommandLocked(lock, DecoderCommand::SEEK);

	while (state == DecoderState::START)
		/* If the decoder falls back to DecoderState::START,
		   this means that our SEEK command arrived too late,
		   and the decoder had meanwhile finished decoding and
		   went idle.  Our SEEK command is finished, but that
		   means only that the decoder thread has launched the
		   decoder.  To work around illegal states, we wait
		   until the decoder plugin has become ready.  This is
		   a kludge, built on top of the "late seek" kludge.
		   Not exactly elegant, sorry. */
		WaitForDecoder(lock);

	if (seek_error)
		throw std::runtime_error("Decoder failed to seek");
}

void
DecoderControl::Quit() noexcept
{
	assert(thread.IsDefined());

	quit = true;
	LockAsynchronousCommand(DecoderCommand::STOP);

	thread.Join();
}

void
DecoderControl::CycleMixRamp() noexcept
{
	previous_mix_ramp = std::move(mix_ramp);
	mix_ramp = {};
}
