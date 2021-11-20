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

#include "MultipleOutputs.hxx"
#include "Client.hxx"
#include "Filtered.hxx"
#include "Defaults.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "filter/Factory.hxx"
#include "config/Block.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringAPI.hxx"

#include <cassert>
#include <stdexcept>

#include <string.h>

MultipleOutputs::MultipleOutputs(AudioOutputClient &_client,
				 MixerListener &_mixer_listener) noexcept
	:client(_client), mixer_listener(_mixer_listener)
{
}

MultipleOutputs::~MultipleOutputs() noexcept
{
	/* parallel destruction */
	for (const auto &i : outputs)
		i->BeginDestroy();
}

static std::unique_ptr<FilteredAudioOutput>
LoadOutput(EventLoop &event_loop, EventLoop &rt_event_loop,
	   const ReplayGainConfig &replay_gain_config,
	   MixerListener &mixer_listener,
	   const ConfigBlock &block,
	   const AudioOutputDefaults &defaults,
	   FilterFactory *filter_factory)
try {
	return audio_output_new(event_loop, rt_event_loop, replay_gain_config, block,
				defaults,
				filter_factory,
				mixer_listener);
} catch (...) {
	if (block.line > 0)
		std::throw_with_nested(FormatRuntimeError("Failed to configure output in line %i",
							  block.line));
	else
		throw;
}

static std::unique_ptr<AudioOutputControl>
LoadOutputControl(EventLoop &event_loop, EventLoop &rt_event_loop,
		  const ReplayGainConfig &replay_gain_config,
		  MixerListener &mixer_listener,
		  AudioOutputClient &client, const ConfigBlock &block,
		  const AudioOutputDefaults &defaults,
		  FilterFactory *filter_factory)
{
	auto output = LoadOutput(event_loop, rt_event_loop,
				 replay_gain_config,
				 mixer_listener,
				 block, defaults, filter_factory);
	return std::make_unique<AudioOutputControl>(std::move(output),
						    client, block);
}

void
MultipleOutputs::Configure(EventLoop &event_loop, EventLoop &rt_event_loop,
			   const ConfigData &config,
			   const ReplayGainConfig &replay_gain_config)
{
	const AudioOutputDefaults defaults(config);
	FilterFactory filter_factory(config);

	for (const auto &block : config.GetBlockList(ConfigBlockOption::AUDIO_OUTPUT)) {
		block.SetUsed();
		auto output = LoadOutputControl(event_loop, rt_event_loop,
						replay_gain_config,
						mixer_listener,
						client, block, defaults,
						&filter_factory);
		if (HasName(output->GetName()))
			throw FormatRuntimeError("output devices with identical "
						 "names: %s", output->GetName());

		outputs.emplace_back(std::move(output));
	}

	if (outputs.empty()) {
		/* auto-detect device */
		const ConfigBlock empty;
		outputs.emplace_back(LoadOutputControl(event_loop,
						       rt_event_loop,
						       replay_gain_config,
						       mixer_listener,
						       client, empty, defaults,
						       nullptr));
	}
}

AudioOutputControl *
MultipleOutputs::FindByName(const char *name) noexcept
{
	for (const auto &i : outputs)
		if (StringIsEqual(i->GetName(), name))
			return i.get();

	return nullptr;
}

void
MultipleOutputs::AddMoveFrom(AudioOutputControl &&src,
			     bool enable) noexcept
{
	// TODO: this operation needs to be protected with a mutex
	outputs.push_back(std::make_unique<AudioOutputControl>(std::move(src),
							       client));

	outputs.back()->LockSetEnabled(enable);

	client.ApplyEnabled();
}

void
MultipleOutputs::EnableDisable()
{
	/* parallel execution */

	for (const auto &ao : outputs)
		ao->LockEnableDisableAsync();

	WaitAll();
}

void
MultipleOutputs::WaitAll() noexcept
{
	for (const auto &ao : outputs)
		ao->LockWaitForCommand();
}

void
MultipleOutputs::AllowPlay() noexcept
{
	for (const auto &ao : outputs)
		ao->LockAllowPlay();
}

bool
MultipleOutputs::Update(bool force) noexcept
{
	bool ret = false;

	if (!IsOpen())
		return false;

	for (const auto &ao : outputs)
		ret = ao->LockUpdate(input_audio_format, *pipe, force)
			|| ret;

	return ret;
}

void
MultipleOutputs::SetReplayGainMode(ReplayGainMode mode) noexcept
{
	for (const auto &ao : outputs)
		ao->SetReplayGainMode(mode);
}

void
MultipleOutputs::Play(MusicChunkPtr chunk)
{
	assert(pipe != nullptr);
	assert(chunk != nullptr);
	assert(chunk->CheckFormat(input_audio_format));

	if (!Update(false))
		/* TODO: obtain real error */
		throw std::runtime_error("Failed to open audio output");

	pipe->Push(std::move(chunk));

	for (const auto &ao : outputs)
		ao->LockPlay();
}

void
MultipleOutputs::Open(const AudioFormat audio_format)
{
	bool ret = false, enabled = false;

	/* the audio format must be the same as existing chunks in the
	   pipe */
	assert(pipe == nullptr || pipe->CheckFormat(audio_format));

	if (pipe == nullptr)
		pipe = std::make_unique<MusicPipe>();
	else
		/* if the pipe hasn't been cleared, the the audio
		   format must not have changed */
		assert(pipe->IsEmpty() || audio_format == input_audio_format);

	input_audio_format = audio_format;

	EnableDisable();
	Update(true);

	std::exception_ptr first_error;

	for (const auto &ao : outputs) {
		const std::scoped_lock<Mutex> lock(ao->mutex);

		if (ao->IsEnabled())
			enabled = true;

		if (ao->IsOpen())
			ret = true;
		else if (!first_error)
			first_error = ao->GetLastError();
	}

	if (!enabled) {
		/* close all devices if there was an error */
		Close();
		throw std::runtime_error("All audio outputs are disabled");
	} else if (!ret) {
		/* close all devices if there was an error */
		Close();

		if (first_error)
			/* we have details, so throw that */
			std::rethrow_exception(first_error);
		else
			throw std::runtime_error("Failed to open audio output");
	}
}

bool
MultipleOutputs::IsChunkConsumed(const MusicChunk *chunk) const noexcept
{
	return std::all_of(outputs.begin(), outputs.end(), [chunk](const auto &ao) {
		return ao->LockIsChunkConsumed(*chunk); });
}

unsigned
MultipleOutputs::CheckPipe() noexcept
{
	const MusicChunk *chunk;

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

		const bool is_tail = chunk->next == nullptr;
		if (is_tail)
			/* this is the tail of the pipe - clear the
			   chunk reference in all outputs */
			for (const auto &ao : outputs)
				ao->LockClearTailChunk(*chunk);

		/* remove the chunk from the pipe */
		const auto shifted = pipe->Shift();
		assert(shifted.get() == chunk);

		if (is_tail)
			/* resume playback which has been suspended by
			   LockClearTailChunk() */
			for (const auto &ao : outputs)
				ao->LockAllowPlay();

		/* chunk is automatically returned to the buffer by
		   ~MusicChunkPtr() */
	}

	return 0;
}

void
MultipleOutputs::Pause() noexcept
{
	Update(false);

	for (const auto &ao : outputs)
		ao->LockPauseAsync();

	WaitAll();
}

void
MultipleOutputs::Drain() noexcept
{
	for (const auto &ao : outputs)
		ao->LockDrainAsync();

	WaitAll();
}

void
MultipleOutputs::Cancel() noexcept
{
	/* send the cancel() command to all audio outputs */

	for (const auto &ao : outputs)
		ao->LockCancelAsync();

	WaitAll();

	/* clear the music pipe and return all chunks to the buffer */

	if (pipe != nullptr)
		pipe->Clear();

	/* the audio outputs are now waiting for a signal, to
	   synchronize the cleared music pipe */

	AllowPlay();

	/* invalidate elapsed_time */

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::Close() noexcept
{
	for (const auto &ao : outputs)
		ao->LockCloseWait();

	pipe.reset();

	input_audio_format.Clear();

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::Release() noexcept
{
	for (const auto &ao : outputs)
		ao->LockRelease();

	pipe.reset();

	input_audio_format.Clear();

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::SongBorder() noexcept
{
	/* clear the elapsed_time pointer at the beginning of a new
	   song */
	elapsed_time = SignedSongTime::zero();
}
