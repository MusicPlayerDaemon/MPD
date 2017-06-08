/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Control.hxx"
#include "Internal.hxx"
#include "OutputPlugin.hxx"
#include "Domain.hxx"
#include "mixer/MixerControl.hxx"
#include "notify.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "config/Block.hxx"
#include "Log.hxx"

#include <stdexcept>

#include <assert.h>

/** after a failure, wait this duration before
    automatically reopening the device */
static constexpr PeriodClock::Duration REOPEN_AFTER = std::chrono::seconds(10);

struct notify audio_output_client_notify;

AudioOutputControl::AudioOutputControl(AudioOutput *_output,
				       AudioOutputClient &_client)
	:output(_output), client(_client),
	 thread(BIND_THIS_METHOD(Task)),
	 mutex(output->mutex)
{
}

void
AudioOutputControl::Configure(const ConfigBlock &block)
{
	tags = block.GetBlockValue("tags", true);
	always_on = block.GetBlockValue("always_on", false);
	enabled = block.GetBlockValue("enabled", true);
}

const char *
AudioOutputControl::GetName() const noexcept
{
	return output->GetName();
}

Mixer *
AudioOutputControl::GetMixer() const noexcept
{
	return output->mixer;
}

bool
AudioOutputControl::LockSetEnabled(bool new_value) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	if (new_value == enabled)
		return false;

	enabled = new_value;
	return true;
}

bool
AudioOutputControl::LockToggleEnabled() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	return enabled = !enabled;
}

void
AudioOutputControl::WaitForCommand() noexcept
{
	while (!IsCommandFinished()) {
		mutex.unlock();
		audio_output_client_notify.Wait();
		mutex.lock();
	}
}

void
AudioOutputControl::CommandAsync(Command cmd) noexcept
{
	assert(IsCommandFinished());

	command = cmd;
	cond.signal();
}

void
AudioOutputControl::CommandWait(Command cmd) noexcept
{
	CommandAsync(cmd);
	WaitForCommand();
}

void
AudioOutputControl::LockCommandWait(Command cmd) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	CommandWait(cmd);
}

void
AudioOutputControl::EnableAsync()
{
	if (!thread.IsDefined()) {
		if (output->plugin.enable == nullptr) {
			/* don't bother to start the thread now if the
			   device doesn't even have a enable() method;
			   just assign the variable and we're done */
			really_enabled = true;
			return;
		}

		StartThread();
	}

	CommandAsync(Command::ENABLE);
}

void
AudioOutputControl::DisableAsync() noexcept
{
	if (!thread.IsDefined()) {
		if (output->plugin.disable == nullptr)
			really_enabled = false;
		else
			/* if there's no thread yet, the device cannot
			   be enabled */
			assert(!really_enabled);

		return;
	}

	CommandAsync(Command::DISABLE);
}

void
AudioOutputControl::EnableDisableAsync()
{
	if (enabled == really_enabled)
		return;

	if (enabled)
		EnableAsync();
	else
		DisableAsync();
}

inline bool
AudioOutputControl::Open(const AudioFormat audio_format,
			 const MusicPipe &mp) noexcept
{
	assert(allow_play);
	assert(audio_format.IsValid());

	fail_timer.Reset();

	if (open && audio_format == request.audio_format) {
		assert(request.pipe == &mp || (always_on && pause));

		if (!pause)
			/* already open, already the right parameters
			   - nothing needs to be done */
			return true;
	}

	request.audio_format = audio_format;
	request.pipe = &mp;

	if (!thread.IsDefined())
		StartThread();

	CommandWait(Command::OPEN);
	const bool open2 = open;

	if (open2 && output->mixer != nullptr) {
		try {
			mixer_open(output->mixer);
		} catch (const std::runtime_error &e) {
			FormatError(e, "Failed to open mixer for '%s'",
				    GetName());
		}
	}

	return open2;
}

void
AudioOutputControl::CloseWait() noexcept
{
	assert(allow_play);

	if (output->mixer != nullptr)
		mixer_auto_close(output->mixer);

	assert(!open || !fail_timer.IsDefined());

	if (open)
		CommandWait(Command::CLOSE);
	else
		fail_timer.Reset();
}

bool
AudioOutputControl::LockUpdate(const AudioFormat audio_format,
			       const MusicPipe &mp,
			       bool force) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	if (enabled && really_enabled) {
		if (force || !fail_timer.IsDefined() ||
		    fail_timer.Check(REOPEN_AFTER * 1000)) {
			return Open(audio_format, mp);
		}
	} else if (IsOpen())
		CloseWait();

	return false;
}

bool
AudioOutputControl::IsChunkConsumed(const MusicChunk &chunk) const noexcept
{
	if (!open)
		return true;

	return source.IsChunkConsumed(chunk);
}

bool
AudioOutputControl::LockIsChunkConsumed(const MusicChunk &chunk) const noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	return IsChunkConsumed(chunk);
}

void
AudioOutputControl::LockPlay() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	assert(allow_play);

	if (IsOpen() && !in_playback_loop && !woken_for_play) {
		woken_for_play = true;
		cond.signal();
	}
}

void
AudioOutputControl::LockPauseAsync() noexcept
{
	if (output->mixer != nullptr && output->plugin.pause == nullptr)
		/* the device has no pause mode: close the mixer,
		   unless its "global" flag is set (checked by
		   mixer_auto_close()) */
		mixer_auto_close(output->mixer);

	const std::lock_guard<Mutex> protect(mutex);

	assert(allow_play);
	if (IsOpen())
		CommandAsync(Command::PAUSE);
}

void
AudioOutputControl::LockDrainAsync() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	assert(allow_play);
	if (IsOpen())
		CommandAsync(Command::DRAIN);
}

void
AudioOutputControl::LockCancelAsync() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	if (IsOpen()) {
		allow_play = false;
		CommandAsync(Command::CANCEL);
	}
}

void
AudioOutputControl::LockAllowPlay() noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	allow_play = true;
	if (IsOpen())
		cond.signal();
}

void
AudioOutputControl::LockRelease() noexcept
{
	if (always_on)
		LockPauseAsync();
	else
		LockCloseWait();
}

void
AudioOutputControl::LockCloseWait() noexcept
{
	assert(!open || !fail_timer.IsDefined());

	const std::lock_guard<Mutex> protect(mutex);
	CloseWait();
}

void
AudioOutputControl::StopThread() noexcept
{
	assert(thread.IsDefined());
	assert(allow_play);

	LockCommandWait(Command::KILL);
	thread.Join();
}

void
AudioOutput::BeginDestroy() noexcept
{
	if (mixer != nullptr)
		mixer_auto_close(mixer);
}

void
AudioOutputControl::BeginDestroy() noexcept
{
	output->BeginDestroy();

	if (thread.IsDefined()) {
		const std::lock_guard<Mutex> protect(mutex);
		CommandAsync(Command::KILL);
	}
}

void
AudioOutput::FinishDestroy() noexcept
{
	audio_output_free(this);
}

void
AudioOutputControl::FinishDestroy() noexcept
{
	if (thread.IsDefined())
		thread.Join();

	output->FinishDestroy();
	output = nullptr;
}
