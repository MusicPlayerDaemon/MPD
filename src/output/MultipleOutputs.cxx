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
#include "OutputControl.hxx"
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

MultipleOutputs::MultipleOutputs()
	:buffer(nullptr), pipe(nullptr),
	 elapsed_time(-1)
{
}

MultipleOutputs::~MultipleOutputs()
{
	for (auto i : outputs) {
		audio_output_disable(i);
		audio_output_finish(i);
	}
}

static AudioOutput *
LoadOutput(PlayerControl &pc, const config_param &param)
{
	Error error;
	AudioOutput *output = audio_output_new(param, pc, error);
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
MultipleOutputs::Configure(PlayerControl &pc)
{
	const config_param *param = nullptr;
	while ((param = config_get_next_param(CONF_AUDIO_OUTPUT,
					      param)) != nullptr) {
		auto output = LoadOutput(pc, *param);
		if (FindByName(output->name) != nullptr)
			FormatFatalError("output devices with identical "
					 "names: %s", output->name);

		outputs.push_back(output);
	}

	if (outputs.empty()) {
		/* auto-detect device */
		const config_param empty;
		auto output = LoadOutput(pc, empty);
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
				audio_output_enable(ao);
			else
				audio_output_disable(ao);
		}
	}
}

bool
MultipleOutputs::AllFinished() const
{
	for (auto ao : outputs) {
		const ScopeLock protect(ao->mutex);
		if (audio_output_is_open(ao) &&
		    !audio_output_command_is_finished(ao))
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
		audio_output_allow_play(ao);
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
		ret = audio_output_update(ao, input_audio_format, *pipe)
			|| ret;

	return ret;
}

void
MultipleOutputs::SetReplayGainMode(ReplayGainMode mode)
{
	for (auto ao : outputs)
		audio_output_set_replay_gain_mode(ao, mode);
}

bool
MultipleOutputs::Play(music_chunk *chunk, Error &error)
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
		audio_output_play(ao);

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
		     const struct music_chunk *chunk)
{
	if (!ao->open)
		return true;

	if (ao->chunk == nullptr)
		return false;

	assert(chunk == ao->chunk || pipe->Contains(ao->chunk));

	if (chunk != ao->chunk) {
		assert(chunk->next != nullptr);
		return true;
	}

	return ao->chunk_finished && chunk->next == nullptr;
}

bool
MultipleOutputs::IsChunkConsumed(const music_chunk *chunk) const
{
	for (auto ao : outputs) {
		const ScopeLock protect(ao->mutex);
		if (!chunk_is_consumed_in(ao, pipe, chunk))
			return false;
	}

	return true;
}

inline void
MultipleOutputs::ClearTailChunk(gcc_unused const struct music_chunk *chunk,
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

		assert(ao->chunk == chunk);
		assert(ao->chunk_finished);
		ao->chunk = nullptr;
	}
}

unsigned
MultipleOutputs::Check()
{
	const struct music_chunk *chunk;
	bool is_tail;
	struct music_chunk *shifted;
	bool locked[outputs.size()];

	assert(buffer != nullptr);
	assert(pipe != nullptr);

	while ((chunk = pipe->Peek()) != nullptr) {
		assert(!pipe->IsEmpty());

		if (!IsChunkConsumed(chunk))
			/* at least one output is not finished playing
			   this chunk */
			return pipe->GetSize();

		if (chunk->length > 0 && chunk->times >= 0.0)
			/* only update elapsed_time if the chunk
			   provides a defined value */
			elapsed_time = chunk->times;

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
		audio_output_pause(ao);

	WaitAll();
}

void
MultipleOutputs::Drain()
{
	for (auto ao : outputs)
		audio_output_drain_async(ao);

	WaitAll();
}

void
MultipleOutputs::Cancel()
{
	/* send the cancel() command to all audio outputs */

	for (auto ao : outputs)
		audio_output_cancel(ao);

	WaitAll();

	/* clear the music pipe and return all chunks to the buffer */

	if (pipe != nullptr)
		pipe->Clear(*buffer);

	/* the audio outputs are now waiting for a signal, to
	   synchronize the cleared music pipe */

	AllowPlay();

	/* invalidate elapsed_time */

	elapsed_time = -1.0;
}

void
MultipleOutputs::Close()
{
	for (auto ao : outputs)
		audio_output_close(ao);

	if (pipe != nullptr) {
		assert(buffer != nullptr);

		pipe->Clear(*buffer);
		delete pipe;
		pipe = nullptr;
	}

	buffer = nullptr;

	input_audio_format.Clear();

	elapsed_time = -1.0;
}

void
MultipleOutputs::Release()
{
	for (auto ao : outputs)
		audio_output_release(ao);

	if (pipe != nullptr) {
		assert(buffer != nullptr);

		pipe->Clear(*buffer);
		delete pipe;
		pipe = nullptr;
	}

	buffer = nullptr;

	input_audio_format.Clear();

	elapsed_time = -1.0;
}

void
MultipleOutputs::SongBorder()
{
	/* clear the elapsed_time pointer at the beginning of a new
	   song */
	elapsed_time = 0.0;
}
