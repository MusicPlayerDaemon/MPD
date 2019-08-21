/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
#include "Filtered.hxx"
#include "Client.hxx"
#include "Domain.hxx"
#include "thread/Util.hxx"
#include "thread/Slack.hxx"
#include "thread/Name.hxx"
#include "util/StringBuffer.hxx"
#include "util/ScopeExit.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

void
AudioOutputControl::CommandFinished() noexcept
{
	assert(command != Command::NONE);
	command = Command::NONE;

	client_cond.notify_one();
}

inline void
AudioOutputControl::InternalOpen2(const AudioFormat in_audio_format)
{
	assert(in_audio_format.IsValid());

	const auto cf = in_audio_format.WithMask(output->config_audio_format);

	if (open && cf != output->filter_audio_format)
		/* if the filter's output format changes, the output
		   must be reopened as well */
		InternalCloseOutput(true);

	output->filter_audio_format = cf;

	if (!open) {
		{
			const ScopeUnlock unlock(mutex);
			output->OpenOutputAndConvert(output->filter_audio_format);
		}

		open = true;
	} else if (in_audio_format != output->out_audio_format) {
		/* reconfigure the final ConvertFilter for its new
		   input AudioFormat */

		try {
			output->ConfigureConvertFilter();
		} catch (...) {
			open = false;

			{
				const ScopeUnlock unlock(mutex);
				output->CloseOutput(false);
			}

			throw;
		}
	}

	{
		const ScopeUnlock unlock(mutex);
		output->OpenSoftwareMixer();
	}
}

inline bool
AudioOutputControl::InternalEnable() noexcept
{
	if (really_enabled)
		/* already enabled */
		return true;

	last_error = nullptr;

	try {
		{
			const ScopeUnlock unlock(mutex);
			output->Enable();
		}

		really_enabled = true;
		return true;
	} catch (...) {
		LogError(std::current_exception());
		Failure(std::current_exception());
		return false;
	}
}

inline void
AudioOutputControl::InternalDisable() noexcept
{
	if (!really_enabled)
		return;

	InternalCheckClose(false);

	really_enabled = false;

	const ScopeUnlock unlock(mutex);
	output->Disable();
}

inline void
AudioOutputControl::InternalOpen(const AudioFormat in_audio_format,
				 const MusicPipe &pipe) noexcept
{
	/* enable the device (just in case the last enable has failed) */
	if (!InternalEnable())
		return;

	last_error = nullptr;
	fail_timer.Reset();
	skip_delay = true;

	AudioFormat f;

	try {
		try {
			f = source.Open(in_audio_format, pipe,
					output->prepared_replay_gain_filter.get(),
					output->prepared_other_replay_gain_filter.get(),
					*output->prepared_filter);
		} catch (...) {
			std::throw_with_nested(FormatRuntimeError("Failed to open filter for %s",
								  GetLogName()));
		}

		try {
			InternalOpen2(f);
		} catch (...) {
			source.Close();
			throw;
		}
	} catch (...) {
		LogError(std::current_exception());
		Failure(std::current_exception());
		return;
	}

	if (f != in_audio_format || f != output->out_audio_format)
		FormatDebug(output_domain, "converting in=%s -> f=%s -> out=%s",
			    ToString(in_audio_format).c_str(),
			    ToString(f).c_str(),
			    ToString(output->out_audio_format).c_str());
}

inline void
AudioOutputControl::InternalCloseOutput(bool drain) noexcept
{
	assert(IsOpen());

	open = false;

	const ScopeUnlock unlock(mutex);
	output->CloseOutput(drain);
}

inline void
AudioOutputControl::InternalClose(bool drain) noexcept
{
	assert(IsOpen());

	open = false;

	{
		const ScopeUnlock unlock(mutex);
		output->Close(drain);
	}

	source.Close();
}

inline void
AudioOutputControl::InternalCheckClose(bool drain) noexcept
{
	if (IsOpen())
		InternalClose(drain);
}

/**
 * Wait until the output's delay reaches zero.
 *
 * @return true if playback should be continued, false if a command
 * was issued
 */
inline bool
AudioOutputControl::WaitForDelay(std::unique_lock<Mutex> &lock) noexcept
{
	while (true) {
		const auto delay = output->Delay();
		if (delay <= std::chrono::steady_clock::duration::zero())
			return true;

		(void)wake_cond.wait_for(lock, delay);

		if (command != Command::NONE)
			return false;
	}
}

bool
AudioOutputControl::FillSourceOrClose() noexcept
try {
	return source.Fill(mutex);
} catch (...) {
	FormatError(std::current_exception(),
		    "Failed to filter for %s", GetLogName());
	InternalCloseError(std::current_exception());
	return false;
}

inline bool
AudioOutputControl::PlayChunk(std::unique_lock<Mutex> &lock) noexcept
{
	// ensure pending tags are flushed in all cases
	const auto *tag = source.ReadTag();
	if (tags && tag != nullptr) {
		const ScopeUnlock unlock(mutex);
		try {
			output->SendTag(*tag);
		} catch (...) {
			FormatError(std::current_exception(),
				    "Failed to send tag to %s",
				    GetLogName());
		}
	}

	while (command == Command::NONE) {
		const auto data = source.PeekData();
		if (data.empty())
			break;

		if (skip_delay)
			skip_delay = false;
		else if (!WaitForDelay(lock))
			break;

		size_t nbytes;

		try {
			const ScopeUnlock unlock(mutex);
			nbytes = output->Play(data.data, data.size);
			assert(nbytes > 0);
			assert(nbytes <= data.size);
		} catch (...) {
			FormatError(std::current_exception(),
				    "Failed to play on %s", GetLogName());
			InternalCloseError(std::current_exception());
			return false;
		}

		assert(nbytes % output->out_audio_format.GetFrameSize() == 0);

		source.ConsumeData(nbytes);
	}

	return true;
}

inline bool
AudioOutputControl::InternalPlay(std::unique_lock<Mutex> &lock) noexcept
{
	if (!FillSourceOrClose())
		/* no chunk available */
		return false;

	assert(!in_playback_loop);
	in_playback_loop = true;

	AtScopeExit(this) {
		assert(in_playback_loop);
		in_playback_loop = false;
	};

	unsigned n = 0;

	do {
		if (command != Command::NONE)
			return true;

		if (++n >= 64) {
			/* wake up the player every now and then to
			   give it a chance to refill the pipe before
			   it runs empty */
			const ScopeUnlock unlock(mutex);
			client.ChunksConsumed();
			n = 0;
		}

		if (!PlayChunk(lock))
			break;
	} while (FillSourceOrClose());

	const ScopeUnlock unlock(mutex);
	client.ChunksConsumed();

	return true;
}

inline void
AudioOutputControl::InternalPause(std::unique_lock<Mutex> &lock) noexcept
{
	{
		const ScopeUnlock unlock(mutex);
		output->BeginPause();
	}

	pause = true;

	CommandFinished();

	do {
		if (!WaitForDelay(lock))
			break;

		bool success;
		{
			const ScopeUnlock unlock(mutex);
			success = output->IteratePause();
		}

		if (!success) {
			InternalClose(false);
			break;
		}
	} while (command == Command::NONE);

	pause = false;

	{
		const ScopeUnlock unlock(mutex);
		output->EndPause();
	}

	skip_delay = true;
}

static void
PlayFull(FilteredAudioOutput &output, ConstBuffer<void> _buffer)
{
	auto buffer = ConstBuffer<uint8_t>::FromVoid(_buffer);

	while (!buffer.empty()) {
		size_t nbytes = output.Play(buffer.data, buffer.size);
		assert(nbytes > 0);

		buffer.skip_front(nbytes);
	}

}

inline void
AudioOutputControl::InternalDrain() noexcept
{
	try {
		/* flush the filter and play its remaining output */

		const ScopeUnlock unlock(mutex);

		while (true) {
			auto buffer = source.Flush();
			if (buffer.IsNull())
				break;

			PlayFull(*output, buffer);
		}

		output->Drain();
	} catch (...) {
		FormatError(std::current_exception(),
			    "Failed to flush filter on %s", GetLogName());
		InternalCloseError(std::current_exception());
		return;
	}
}

void
AudioOutputControl::Task() noexcept
{
	FormatThreadName("output:%s", GetName());

	try {
		SetThreadRealtime();
	} catch (...) {
		Log(LogLevel::INFO, std::current_exception(),
		    "OutputThread could not get realtime scheduling, continuing anyway");
	}

	SetThreadTimerSlack(std::chrono::microseconds(100));

	std::unique_lock<Mutex> lock(mutex);

	while (true) {
		switch (command) {
		case Command::NONE:
			break;

		case Command::ENABLE:
			InternalEnable();
			CommandFinished();
			break;

		case Command::DISABLE:
			InternalDisable();
			CommandFinished();
			break;

		case Command::OPEN:
			InternalOpen(request.audio_format, *request.pipe);
			CommandFinished();
			break;

		case Command::CLOSE:
			InternalCheckClose(false);
			CommandFinished();
			break;

		case Command::PAUSE:
			if (!open) {
				/* the output has failed after
				   the PAUSE command was submitted; bail
				   out */
				CommandFinished();
				break;
			}

			InternalPause(lock);
			/* don't "break" here: this might cause
			   Play() to be called when command==CLOSE
			   ends the paused state - "continue" checks
			   the new command first */
			continue;

		case Command::RELEASE:
			if (!open) {
				/* the output has failed after
				   the RELEASE command was submitted; bail
				   out */
				CommandFinished();
				break;
			}

			if (always_on) {
				/* in "always_on" mode, the output is
				   paused instead of being closed;
				   however we need to flush the
				   AudioOutputSource because its data
				   have been invalidated by stopping
				   the actual playback */
				source.Cancel();
				InternalPause(lock);
			} else {
				InternalClose(false);
				CommandFinished();
			}

			/* don't "break" here: this might cause
			   Play() to be called when command==CLOSE
			   ends the paused state - "continue" checks
			   the new command first */
			continue;

		case Command::DRAIN:
			if (open)
				InternalDrain();

			CommandFinished();
			continue;

		case Command::CANCEL:
			source.Cancel();

			if (open) {
				const ScopeUnlock unlock(mutex);
				output->Cancel();
			}

			CommandFinished();
			continue;

		case Command::KILL:
			InternalDisable();
			source.Cancel();
			CommandFinished();
			return;
		}

		if (open && allow_play && InternalPlay(lock))
			/* don't wait for an event if there are more
			   chunks in the pipe */
			continue;

		if (command == Command::NONE) {
			woken_for_play = false;
			wake_cond.wait(lock);
		}
	}
}

void
AudioOutputControl::StartThread()
{
	assert(command == Command::NONE);

	const ScopeUnlock unlock(mutex);
	thread.Start();
}
