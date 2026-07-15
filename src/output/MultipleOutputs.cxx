// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MultipleOutputs.hxx"
#include "AllOutputs.hxx"
#include "Client.hxx"
#include "Control.hxx"
#include "Filtered.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "util/StringAPI.hxx"

#include <algorithm> // for std::all_of()
#include <cassert>
#include <stdexcept>

#include <string.h>

MultipleOutputs::MultipleOutputs(AllOutputs &_all_outputs,
				 AudioOutputClient &_client,
				 MixerListener &_mixer_listener) noexcept
	:all_outputs(_all_outputs),
	 client(_client),
	 mixer_listener(_mixer_listener)
{
}

MultipleOutputs::~MultipleOutputs() noexcept = default;

AudioOutputControl *
MultipleOutputs::FindByName(const std::string_view name) noexcept
{
	for (auto &ao : outputs)
		if (name == ao.GetName())
			return &ao;

	return nullptr;
}

bool
MultipleOutputs::Owns(const AudioOutputControl &ao) const noexcept
{
	return ao.GetMixerListener() == &mixer_listener;
}

void
MultipleOutputs::AcquireAll(ReplayGainMode replay_gain_mode) noexcept
{
	assert(outputs.empty());

	for (unsigned i = 0, n = all_outputs.Size(); i != n; ++i) {
		auto &ao = all_outputs.Get(i);
		if (ao.GetMixerListener() == nullptr) {
			outputs.push_back(ao);
			ao.LockSetMixerListener(mixer_listener);
			ao.SetClient(client);
			ao.SetReplayGainMode(replay_gain_mode);
		}
	}
}

void
MultipleOutputs::AcquireOwnership(AudioOutputControl &ao, bool enable,
				  ReplayGainMode replay_gain_mode) noexcept
{
	assert(!Owns(ao));

	{
		const std::lock_guard lock{mutex};
		outputs.push_back(ao);
	}

	ao.LockSetMixerListener(mixer_listener);
	ao.SetClient(client);
	ao.LockSetEnabled(enable);
	ao.SetReplayGainMode(replay_gain_mode);

	client.ApplyEnabled();
}

void
MultipleOutputs::ReleaseOwnership(AudioOutputControl &ao) noexcept
{
	assert(Owns(ao));

	ao.LockDisable();

	const std::lock_guard lock{mutex};
	outputs.erase(outputs.iterator_to(ao));
}

void
MultipleOutputs::_EnableDisable()
{
	/* parallel execution */

	for (auto &ao : outputs)
		ao.LockEnableDisableAsync();

	WaitAll();
}

void
MultipleOutputs::EnableDisable()
{
	const std::lock_guard lock{mutex};
	_EnableDisable();
}

void
MultipleOutputs::WaitAll() noexcept
{
	for (auto &ao : outputs)
		ao.LockWaitForCommand();
}

inline void
MultipleOutputs::AllowPlay() noexcept
{
	for (auto &ao : outputs)
		ao.LockAllowPlay();
}

bool
MultipleOutputs::_Update(bool force) noexcept
{
	bool ret = false;

	if (!IsOpen())
		return false;

	for (auto &ao : outputs)
		ret = ao.LockUpdate(input_audio_format, *pipe, force)
			|| ret;

	return ret;
}

bool
MultipleOutputs::Update(bool force) noexcept
{
	const std::lock_guard lock{mutex};
	return _Update(force);
}

void
MultipleOutputs::SetReplayGainMode(ReplayGainMode mode) noexcept
{
	const std::lock_guard lock{mutex};
	for (auto &ao : outputs)
		ao.SetReplayGainMode(mode);
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

	const std::lock_guard lock{mutex};
	for (auto &ao : outputs)
		ao.LockPlay();
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

	const std::lock_guard outputs_lock{mutex};

	_EnableDisable();
	_Update(true);

	std::exception_ptr first_error;

	for (auto &ao : outputs) {
		const std::lock_guard lock{ao.mutex};

		/* can't play on this device even if it's enabled */
		if (ao.AlwaysOff())
			continue;

		if (ao.IsEnabled())
			enabled = true;

		if (ao.IsOpen())
			ret = true;
		else if (!first_error)
			first_error = ao.GetLastError();
	}

	if (!enabled) {
		/* close all devices if there was an error */
		_Close();
		throw std::runtime_error("All audio outputs are disabled");
	} else if (!ret) {
		/* close all devices if there was an error */
		_Close();

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
	const std::lock_guard lock{mutex};

	return std::all_of(outputs.begin(), outputs.end(), [chunk](const auto &ao) {
		return ao.LockIsChunkConsumed(*chunk); });
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
		if (is_tail) {
			/* this is the tail of the pipe - clear the
			   chunk reference in all outputs */
			const std::lock_guard lock{mutex};
			for (auto &ao : outputs)
				ao.LockClearTailChunk(*chunk);
		}

		/* remove the chunk from the pipe */
		const auto shifted = pipe->Shift();
		assert(shifted.get() == chunk);

		if (is_tail) {
			/* resume playback which has been suspended by
			   LockClearTailChunk() */
			const std::lock_guard lock{mutex};
			AllowPlay();
		}

		/* chunk is automatically returned to the buffer by
		   ~MusicChunkPtr() */
	}

	return 0;
}

void
MultipleOutputs::Pause() noexcept
{
	const std::lock_guard lock{mutex};

	_Update(false);

	for (auto &ao : outputs)
		ao.LockPauseAsync();

	WaitAll();
}

void
MultipleOutputs::Drain() noexcept
{
	const std::lock_guard lock{mutex};

	for (auto &ao : outputs)
		ao.LockDrainAsync();

	WaitAll();
}

void
MultipleOutputs::Cancel() noexcept
{
	const std::lock_guard lock{mutex};

	/* send the cancel() command to all audio outputs */

	for (auto &ao : outputs)
		ao.LockCancelAsync();

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
MultipleOutputs::_Close() noexcept
{
	for (auto &ao : outputs)
		ao.LockCloseWait();

	pipe.reset();

	input_audio_format.Clear();

	elapsed_time = SignedSongTime::Negative();
}

void
MultipleOutputs::Close() noexcept
{
	const std::lock_guard lock{mutex};
	_Close();
}

void
MultipleOutputs::Release() noexcept
{
	{
		const std::lock_guard lock{mutex};
		for (auto &ao : outputs)
			ao.LockRelease();
	}

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
