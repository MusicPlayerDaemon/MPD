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
#include "Log.hxx"

#include <stdexcept>

#include <assert.h>

/** after a failure, wait this duration before
    automatically reopening the device */
static constexpr PeriodClock::Duration REOPEN_AFTER = std::chrono::seconds(10);

struct notify audio_output_client_notify;

AudioOutputControl::AudioOutputControl(AudioOutput *_output)
	:output(_output),
	 thread(BIND_THIS_METHOD(Task)),
	 mutex(output->mutex)
{
}

const char *
AudioOutputControl::GetName() const
{
	return output->GetName();
}

AudioOutputClient &
AudioOutputControl::GetClient()
{
	return *output->client;
}

Mixer *
AudioOutputControl::GetMixer() const
{
	return output->mixer;
}

bool
AudioOutputControl::IsEnabled() const
{
	return output->IsEnabled();
}

bool
AudioOutputControl::LockSetEnabled(bool new_value)
{
	const std::lock_guard<Mutex> protect(mutex);

	if (new_value == output->enabled)
		return false;

	output->enabled = new_value;
	return true;
}

bool
AudioOutputControl::LockToggleEnabled()
{
	const std::lock_guard<Mutex> protect(mutex);
	return output->enabled = !output->enabled;
}

bool
AudioOutputControl::IsOpen() const
{
	return output->IsOpen();
}

void
AudioOutputControl::WaitForCommand()
{
	while (!IsCommandFinished()) {
		mutex.unlock();
		audio_output_client_notify.Wait();
		mutex.lock();
	}
}

void
AudioOutputControl::CommandAsync(Command cmd)
{
	assert(IsCommandFinished());

	command = cmd;
	cond.signal();
}

void
AudioOutputControl::CommandWait(Command cmd)
{
	CommandAsync(cmd);
	WaitForCommand();
}

void
AudioOutputControl::LockCommandWait(Command cmd)
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
			output->really_enabled = true;
			return;
		}

		StartThread();
	}

	CommandAsync(Command::ENABLE);
}

void
AudioOutputControl::DisableAsync()
{
	if (!thread.IsDefined()) {
		if (output->plugin.disable == nullptr)
			output->really_enabled = false;
		else
			/* if there's no thread yet, the device cannot
			   be enabled */
			assert(!output->really_enabled);

		return;
	}

	CommandAsync(Command::DISABLE);
}

void
AudioOutputControl::EnableDisableAsync()
{
	if (output->enabled == output->really_enabled)
		return;

	if (output->enabled)
		EnableAsync();
	else
		DisableAsync();
}

inline bool
AudioOutputControl::Open(const AudioFormat audio_format, const MusicPipe &mp)
{
	assert(allow_play);
	assert(audio_format.IsValid());

	fail_timer.Reset();

	if (output->open && audio_format == request.audio_format) {
		assert(request.pipe == &mp || (output->always_on && output->pause));

		if (!output->pause)
			/* already open, already the right parameters
			   - nothing needs to be done */
			return true;
	}

	request.audio_format = audio_format;
	request.pipe = &mp;

	if (!thread.IsDefined())
		StartThread();

	CommandWait(Command::OPEN);
	const bool open2 = output->open;

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
AudioOutputControl::CloseWait()
{
	assert(allow_play);

	if (output->mixer != nullptr)
		mixer_auto_close(output->mixer);

	assert(!output->open || !fail_timer.IsDefined());

	if (output->open)
		CommandWait(Command::CLOSE);
	else
		fail_timer.Reset();
}

bool
AudioOutputControl::LockUpdate(const AudioFormat audio_format,
			       const MusicPipe &mp,
			       bool force)
{
	const std::lock_guard<Mutex> protect(mutex);

	if (output->enabled && output->really_enabled) {
		if (force || !fail_timer.IsDefined() ||
		    fail_timer.Check(REOPEN_AFTER * 1000)) {
			return Open(audio_format, mp);
		}
	} else if (IsOpen())
		CloseWait();

	return false;
}

bool
AudioOutputControl::LockIsChunkConsumed(const MusicChunk &chunk) const
{
	return output->LockIsChunkConsumed(chunk);
}

void
AudioOutputControl::ClearTailChunk(const MusicChunk &chunk)
{
	output->ClearTailChunk(chunk);
}

void
AudioOutputControl::LockPlay()
{
	const std::lock_guard<Mutex> protect(mutex);

	assert(allow_play);

	if (IsOpen() && !in_playback_loop && !woken_for_play) {
		woken_for_play = true;
		cond.signal();
	}
}

void
AudioOutputControl::LockPauseAsync()
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
AudioOutputControl::LockDrainAsync()
{
	const std::lock_guard<Mutex> protect(mutex);

	assert(allow_play);
	if (IsOpen())
		CommandAsync(Command::DRAIN);
}

void
AudioOutputControl::LockCancelAsync()
{
	const std::lock_guard<Mutex> protect(mutex);

	if (IsOpen()) {
		allow_play = false;
		CommandAsync(Command::CANCEL);
	}
}

void
AudioOutputControl::LockAllowPlay()
{
	const std::lock_guard<Mutex> protect(mutex);

	allow_play = true;
	if (IsOpen())
		cond.signal();
}

void
AudioOutputControl::LockRelease()
{
	if (output->always_on)
		LockPauseAsync();
	else
		LockCloseWait();
}

void
AudioOutputControl::LockCloseWait()
{
	assert(!output->open || !fail_timer.IsDefined());

	const std::lock_guard<Mutex> protect(mutex);
	CloseWait();
}

void
AudioOutputControl::SetReplayGainMode(ReplayGainMode _mode)
{
	return output->SetReplayGainMode(_mode);
}

void
AudioOutputControl::StopThread()
{
	assert(thread.IsDefined());
	assert(allow_play);

	LockCommandWait(Command::KILL);
	thread.Join();
}

void
AudioOutput::BeginDestroy()
{
	if (mixer != nullptr)
		mixer_auto_close(mixer);
}

void
AudioOutputControl::BeginDestroy()
{
	output->BeginDestroy();

	if (thread.IsDefined()) {
		const std::lock_guard<Mutex> protect(mutex);
		CommandAsync(Command::KILL);
	}
}

void
AudioOutput::FinishDestroy()
{
	audio_output_free(this);
}

void
AudioOutputControl::FinishDestroy()
{
	if (thread.IsDefined())
		thread.Join();

	output->FinishDestroy();
	output = nullptr;
}
