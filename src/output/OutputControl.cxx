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
#include "Internal.hxx"
#include "OutputPlugin.hxx"
#include "Domain.hxx"
#include "mixer/MixerControl.hxx"
#include "notify.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <assert.h>

/** after a failure, wait this number of seconds before
    automatically reopening the device */
static constexpr unsigned REOPEN_AFTER = 10;

struct notify audio_output_client_notify;

void
AudioOutput::WaitForCommand()
{
	while (!IsCommandFinished()) {
		mutex.unlock();
		audio_output_client_notify.Wait();
		mutex.lock();
	}
}

void
AudioOutput::CommandAsync(audio_output_command cmd)
{
	assert(IsCommandFinished());

	command = cmd;
	cond.signal();
}

void
AudioOutput::CommandWait(audio_output_command cmd)
{
	CommandAsync(cmd);
	WaitForCommand();
}

void
AudioOutput::LockCommandWait(audio_output_command cmd)
{
	const ScopeLock protect(mutex);
	CommandWait(cmd);
}

void
AudioOutput::SetReplayGainMode(ReplayGainMode mode)
{
	if (replay_gain_filter != nullptr)
		replay_gain_filter_set_mode(replay_gain_filter, mode);
	if (other_replay_gain_filter != nullptr)
		replay_gain_filter_set_mode(other_replay_gain_filter, mode);
}

void
AudioOutput::LockEnableWait()
{
	if (!thread.IsDefined()) {
		if (plugin.enable == nullptr) {
			/* don't bother to start the thread now if the
			   device doesn't even have a enable() method;
			   just assign the variable and we're done */
			really_enabled = true;
			return;
		}

		StartThread();
	}

	LockCommandWait(AO_COMMAND_ENABLE);
}

void
AudioOutput::LockDisableWait()
{
	if (!thread.IsDefined()) {
		if (plugin.disable == nullptr)
			really_enabled = false;
		else
			/* if there's no thread yet, the device cannot
			   be enabled */
			assert(!really_enabled);

		return;
	}

	LockCommandWait(AO_COMMAND_DISABLE);
}

inline bool
AudioOutput::Open(const AudioFormat audio_format, const MusicPipe &mp)
{
	assert(allow_play);
	assert(audio_format.IsValid());

	fail_timer.Reset();

	if (open && audio_format == in_audio_format) {
		assert(pipe == &mp || (always_on && pause));

		if (pause) {
			current_chunk = nullptr;
			pipe = &mp;

			/* unpause with the CANCEL command; this is a
			   hack, but suits well for forcing the thread
			   to leave the ao_pause() thread, and we need
			   to flush the device buffer anyway */

			/* we're not using audio_output_cancel() here,
			   because that function is asynchronous */
			CommandWait(AO_COMMAND_CANCEL);
		}

		return true;
	}

	in_audio_format = audio_format;
	current_chunk = nullptr;

	pipe = &mp;

	if (!thread.IsDefined())
		StartThread();

	CommandWait(open ? AO_COMMAND_REOPEN : AO_COMMAND_OPEN);
	const bool open2 = open;

	if (open2 && mixer != nullptr) {
		Error error;
		if (!mixer_open(mixer, error))
			FormatWarning(output_domain,
				      "Failed to open mixer for '%s'", name);
	}

	return open2;
}

void
AudioOutput::CloseWait()
{
	assert(allow_play);

	if (mixer != nullptr)
		mixer_auto_close(mixer);

	assert(!open || !fail_timer.IsDefined());

	if (open)
		CommandWait(AO_COMMAND_CLOSE);
	else
		fail_timer.Reset();
}

bool
AudioOutput::LockUpdate(const AudioFormat audio_format,
			const MusicPipe &mp)
{
	const ScopeLock protect(mutex);

	if (enabled && really_enabled) {
		if (!fail_timer.IsDefined() ||
		    fail_timer.Check(REOPEN_AFTER * 1000)) {
			return Open(audio_format, mp);
		}
	} else if (IsOpen())
		CloseWait();

	return false;
}

void
AudioOutput::LockPlay()
{
	const ScopeLock protect(mutex);

	assert(allow_play);

	if (IsOpen() && !in_playback_loop && !woken_for_play) {
		woken_for_play = true;
		cond.signal();
	}
}

void
AudioOutput::LockPauseAsync()
{
	if (mixer != nullptr && plugin.pause == nullptr)
		/* the device has no pause mode: close the mixer,
		   unless its "global" flag is set (checked by
		   mixer_auto_close()) */
		mixer_auto_close(mixer);

	const ScopeLock protect(mutex);

	assert(allow_play);
	if (IsOpen())
		CommandAsync(AO_COMMAND_PAUSE);
}

void
AudioOutput::LockDrainAsync()
{
	const ScopeLock protect(mutex);

	assert(allow_play);
	if (IsOpen())
		CommandAsync(AO_COMMAND_DRAIN);
}

void
AudioOutput::LockCancelAsync()
{
	const ScopeLock protect(mutex);

	if (IsOpen()) {
		allow_play = false;
		CommandAsync(AO_COMMAND_CANCEL);
	}
}

void
AudioOutput::LockAllowPlay()
{
	const ScopeLock protect(mutex);

	allow_play = true;
	if (IsOpen())
		cond.signal();
}

void
AudioOutput::LockRelease()
{
	if (always_on)
		LockPauseAsync();
	else
		LockCloseWait();
}

void
AudioOutput::LockCloseWait()
{
	assert(!open || !fail_timer.IsDefined());

	const ScopeLock protect(mutex);
	CloseWait();
}

void
AudioOutput::StopThread()
{
	assert(thread.IsDefined());
	assert(allow_play);

	LockCommandWait(AO_COMMAND_KILL);
	thread.Join();
}

void
AudioOutput::Finish()
{
	LockCloseWait();

	assert(!fail_timer.IsDefined());

	if (thread.IsDefined())
		StopThread();

	audio_output_free(this);
}
