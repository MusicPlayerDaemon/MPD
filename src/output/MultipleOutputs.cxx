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
#include "MultipleOutputs.hxx"
#include "PlayerControl.hxx"
#include "Internal.hxx"
#include "Domain.hxx"
#include "MusicBuffer.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "system/FatalError.hxx"
#include "util/Error.hxx"
#include "config/ConfigData.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigOption.hxx"
#include "notify.hxx"

#include <assert.h>
#include <string.h>

MultipleOutputs::MultipleOutputs(MixerListener &_mixer_listener)
	:mixer_listener(_mixer_listener),
	 input_audio_format(AudioFormat::Undefined()),
	 buffer(nullptr), pipe(nullptr),
	 elapsed_time(SignedSongTime::Negative())
{
}

MultipleOutputs::~MultipleOutputs()
{
	for (auto i : outputs) {
		i->LockDisableWait();
		i->Finish();
	}
}

static AudioOutput *
LoadOutput(EventLoop &event_loop, MixerListener &mixer_listener,
	   PlayerControl &pc, const config_param &param)
{
	Error error;
	AudioOutput *output = audio_output_new(event_loop, param,
					       mixer_listener,
					       pc, error);
	if (output == nullptr) {
		if (param.line > 0)
			FormatFatalError("line %i: %s",
					 param.line,
					 error.GetMessage());
		else
			FatalError(error);
	}

	return output;
}

void
MultipleOutputs::Configure(EventLoop &event_loop, PlayerControl &pc)
{
	for (const config_param *param = config_get_param(CONF_AUDIO_OUTPUT);
	     param != nullptr; param = param->next) {
		auto output = LoadOutput(event_loop, mixer_listener,
					 pc, *param);
		if (FindByName(output->name) != nullptr)
			FormatFatalError("output devices with identical "
					 "names: %s", output->name);

		outputs.push_back(output);
	}

	if (outputs.empty()) {
		/* auto-detect device */
		const config_param empty;
		auto output = LoadOutput(event_loop, mixer_listener,
					 pc, empty);
		outputs.push_back(output);
	}
}

AudioOutput *
MultipleOutputs::FindByName(const char *name) const
{
	for (auto i : outputs)
		if (strcmp(i->name, name) == 0)
			return i;

	return nullptr;
}

void
MultipleOutputs::EnableDisable()
{
	for (auto ao : outputs) {
		bool enabled;

		ao->mutex.lock();
		enabled = ao->really_enabled;
		ao->mutex.unlock();

		if (ao->enabled != enabled) {
			if (ao->enabled)
				ao->LockEnableWait();
			else
				ao->LockDisableWait();
		}
	}
}

bool
MultipleOutputs::AllFinished() const
{
	for (auto ao : outputs) {
		const ScopeLock protect(ao->mutex);
		if (ao->IsOpen() && !ao->IsCommandFinished())
			return false;
	}

	return true;
}

void
MultipleOutputs::WaitAll()
{
	while (!AllFinished())
		audio_output_client_notify.Wait();
}

void
MultipleOutputs::AllowPlay()
{
	for (auto ao : outputs)
		ao->LockAllowPlay();
}

static void
audio_output_reset_reopen(AudioOutput *ao)
{
	const ScopeLock protect(ao->mutex);

	ao->fail_timer.Reset();
}

void
MultipleOutputs::ResetReopen()
{
	for (auto ao : outputs)
		audio_output_reset_reopen(ao);
}

bool
MultipleOutputs::Update()
{
	bool ret = false;

	if (!input_audio_format.IsDefined())
		return false;

	for (auto ao : outputs)
		ret = ao->LockUpdate(input_audio_format, *pipe)
			|| ret;

	return ret;
}

void
MultipleOutputs::SetReplayGainMode(ReplayGainMode mode)
{
	for (auto ao : outputs)
		ao->SetReplayGainMode(mode);
}

bool
MultipleOutputs::Play(MusicChunk *chunk, Error &error)
{
	assert(buffer != nullptr);
	assert(pipe != nullptr);
	assert(chunk != nullptr);
	assert(chunk->CheckFormat(input_audio_format));

	if (!Update()) {
		/* TODO: obtain real error */
		error.Set(output_domain, "Failed to open audio output");
		return false;
	}

	pipe->Push(chunk);

	for (auto ao : outputs)
		ao->LockPlay();

	return true;
}

bool
MultipleOutputs::Open(const AudioFormat audio_format,
		      MusicBuffer &_buffer,
		      Error &error)
{
	bool ret = false, enabled = false;

	assert(buffer == nullptr || buffer == &_buffer);
	assert((pipe == nullptr) == (buffer == nullptr));

	buffer = &_buffer;

	/* the audio format must be the same as existing chunks in the
	   pipe */
	assert(pipe == nullptr || pipe->CheckFormat(audio_format));

	if (pipe == nullptr)
		pipe = new MusicPipe();
	else
		/* if the pipe hasn't been cleared, the the audio
		   format must not have changed */
		assert(pipe->IsEmpty() || audio_format == input_audio_format);

	input_audio_format = audio_format;

	ResetReopen();
	EnableDisable();
	Update();

	for (auto ao : outputs) {
		if (ao->enabled)
			enabled = true;

		if (ao->open)
			ret = true;
	}

	if (!enabled)
		error.Set(output_domain, "All audio outputs are disabled");
	else if (!ret)
		/* TODO: obtain real error */
		error.Set(output_domain, "Failed to open audio output");

	if (!ret)
		/* close all devices if there was an error */
		Close();

	return ret;
}

/**
 * Has the specified audio output already consumed this chunk?
 */
gcc_pure
static bool
chunk_is_consumed_in(const AudioOutput *ao,
		     gcc_unused const MusicPipe *pipe,
		     const MusicChunk *chunk)
{
	if (!ao->open)
		return true;

	if (ao->current_chunk == nullptr)
		return false;

	assert(chunk == ao->current_chunk ||
	       pipe->Contains(ao->current_chunk));

	if (chunk != ao->current_chunk) {
		assert(chunk->next != nullptr);
		return true;
	}

	return ao->current_chunk_finished && chunk->next == nullptr;
}

bool
MultipleOutputs::IsChunkConsumed(const MusicChunk *chunk) const
{
	for (auto ao : outputs) {
		const ScopeLock protect(ao->mutex);
		if (!chunk_is_consumed_in(ao, pipe, chunk))
			return false;
	}

	return true;
}

inline void
MultipleOutputs::ClearTailChunk(gcc_unused const MusicChunk *chunk,
				bool *locked)
{
	assert(chunk->next == nullptr);
	assert(pipe->Contains(chunk));

	for (unsigned i = 0, n = outputs.size(); i != n; ++i) {
		AudioOutput *ao = outputs[i];

		/* this mutex will be unlocked by the caller when it's
		   ready */
		ao->mutex.lock();
		locked[i] = ao->open;

		if (!locked[i]) {
			ao->mutex.unlock();
			continue;
		}

		assert(ao->current_chunk == chunk);
		assert(ao->current_chunk_finished);
		ao->current_chunk = nullptr;
	}
}

unsigned
MultipleOutputs::Check()
{
	const MusicChunk *chunk;
	bool is_tail;
	MusicChunk *shifted;
	bool locked[outputs.size()];

	assert(buffer != nullptr);
	assert(pipe != nullptr);

	while ((chunk = pipe->Peek()) != nullptr) {
		assert(!pipe->IsEmpty());

		if (!IsChunkConsumed(chunk))
			/* at least one output is not finished playing
			   this chunk */
			return pipe->GetSize();

		if (chunk->length > 0 && !chunk->time.IsNegative())
			/* only update elapsed_time if the chunk
			   provides a defined value */
			elapsed_time = chunk->time;

		is_tail = chunk->next == nullptr;
		if (is_tail)
			/* this is the tail of the pipe - clear the
			   chunk reference in all outputs */
			ClearTailChunk(chunk, locked);

		/* remove the chunk from the pipe */
		shifted = pipe->Shift();
		assert(shifted == chunk);

		if (is_tail)
			/* unlock all audio outputs which were locked
			   by clear_tail_chunk() */
			for (unsigned i = 0, n = outputs.size(); i != n; ++i)
				if (locked[i])
					outputs[i]->mutex.unlock();

		/* return the chunk to the buffer */
		buffer->Return(shifted);
	}

	return 0;
}

bool
MultipleOutputs::Wait(PlayerControl &pc, unsigned threshold)
{
	pc.Lock();

	if (Check() < threshold) {
		pc.Unlock();
		return true;
	}

	pc.Wait();
	pc.Unlock();

	return Check() < threshold;
}

void
MultipleOutputs::Pause()
{
	Update();

	for (auto ao : outputs)
		ao->LockPauseAsync();

	WaitAll();
}

void
MultipleOutputs::Drain()
{
	for (auto ao : outputs)
		ao->LockDrainAsync();

	WaitAll();
}

void
MultipleOutputs::Cancel()
{
	/* send the cancel() command to all audio outputs */

	for (auto ao : outputs)
		ao->LockCancelAsync();

	WaitAll();

	/* clear the music pipe and return all chunks to the buffer */

	if (pipe != nullptr)
		pipe->Clear(*buffer);

	/* the audio outputs are now waiting for a signal, to
	   synchronize the cleared music pipe */

	AllowPlay();

	/* invalidate elapsed_time */

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::Close()
{
	for (auto ao : outputs)
		ao->LockCloseWait();

	if (pipe != nullptr) {
		assert(buffer != nullptr);

		pipe->Clear(*buffer);
		delete pipe;
		pipe = nullptr;
	}

	buffer = nullptr;

	input_audio_format.Clear();

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::Release()
{
	for (auto ao : outputs)
		ao->LockRelease();

	if (pipe != nullptr) {
		assert(buffer != nullptr);

		pipe->Clear(*buffer);
		delete pipe;
		pipe = nullptr;
	}

	buffer = nullptr;

	input_audio_format.Clear();

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::SongBorder()
{
	/* clear the elapsed_time pointer at the beginning of a new
	   song */
	elapsed_time = SignedSongTime::zero();
}
