
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
#include "OutputControl.hxx"
#include "OutputThread.hxx"
#include "OutputInternal.hxx"
#include "OutputPlugin.hxx"
#include "OutputError.hxx"
#include "MixerControl.hxx"
#include "notify.hxx"
#include "filter/ReplayGainFilterPlugin.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <assert.h>

/** after a failure, wait this number of seconds before
    automatically reopening the device */
static constexpr unsigned REOPEN_AFTER = 10;

struct notify audio_output_client_notify;

/**
 * Waits for command completion.
 *
 * @param ao the #audio_output instance; must be locked
 */
static void ao_command_wait(struct audio_output *ao)
{
	while (ao->command != AO_COMMAND_NONE) {
		ao->mutex.unlock();
		audio_output_client_notify.Wait();
		ao->mutex.lock();
	}
}

/**
 * Sends a command to the #audio_output object, but does not wait for
 * completion.
 *
 * @param ao the #audio_output instance; must be locked
 */
static void ao_command_async(struct audio_output *ao,
			     enum audio_output_command cmd)
{
	assert(ao->command == AO_COMMAND_NONE);
	ao->command = cmd;
	ao->cond.signal();
}

/**
 * Sends a command to the #audio_output object and waits for
 * completion.
 *
 * @param ao the #audio_output instance; must be locked
 */
static void
ao_command(struct audio_output *ao, enum audio_output_command cmd)
{
	ao_command_async(ao, cmd);
	ao_command_wait(ao);
}

/**
 * Lock the #audio_output object and execute the command
 * synchronously.
 */
static void
ao_lock_command(struct audio_output *ao, enum audio_output_command cmd)
{
	const ScopeLock protect(ao->mutex);
	ao_command(ao, cmd);
}

void
audio_output_set_replay_gain_mode(struct audio_output *ao,
				  ReplayGainMode mode)
{
	if (ao->replay_gain_filter != nullptr)
		replay_gain_filter_set_mode(ao->replay_gain_filter, mode);
	if (ao->other_replay_gain_filter != nullptr)
		replay_gain_filter_set_mode(ao->other_replay_gain_filter, mode);
}

void
audio_output_enable(struct audio_output *ao)
{
	if (!ao->thread.IsDefined()) {
		if (ao->plugin->enable == nullptr) {
			/* don't bother to start the thread now if the
			   device doesn't even have a enable() method;
			   just assign the variable and we're done */
			ao->really_enabled = true;
			return;
		}

		audio_output_thread_start(ao);
	}

	ao_lock_command(ao, AO_COMMAND_ENABLE);
}

void
audio_output_disable(struct audio_output *ao)
{
	if (!ao->thread.IsDefined()) {
		if (ao->plugin->disable == nullptr)
			ao->really_enabled = false;
		else
			/* if there's no thread yet, the device cannot
			   be enabled */
			assert(!ao->really_enabled);

		return;
	}

	ao_lock_command(ao, AO_COMMAND_DISABLE);
}

/**
 * Object must be locked (and unlocked) by the caller.
 */
static bool
audio_output_open(struct audio_output *ao,
		  const AudioFormat audio_format,
		  const MusicPipe &mp)
{
	bool open;

	assert(ao != nullptr);
	assert(ao->allow_play);
	assert(audio_format.IsValid());

	ao->fail_timer.Reset();

	if (ao->open && audio_format == ao->in_audio_format) {
		assert(ao->pipe == &mp ||
		       (ao->always_on && ao->pause));

		if (ao->pause) {
			ao->chunk = nullptr;
			ao->pipe = &mp;

			/* unpause with the CANCEL command; this is a
			   hack, but suits well for forcing the thread
			   to leave the ao_pause() thread, and we need
			   to flush the device buffer anyway */

			/* we're not using audio_output_cancel() here,
			   because that function is asynchronous */
			ao_command(ao, AO_COMMAND_CANCEL);
		}

		return true;
	}

	ao->in_audio_format = audio_format;
	ao->chunk = nullptr;

	ao->pipe = &mp;

	if (!ao->thread.IsDefined())
		audio_output_thread_start(ao);

	ao_command(ao, ao->open ? AO_COMMAND_REOPEN : AO_COMMAND_OPEN);
	open = ao->open;

	if (open && ao->mixer != nullptr) {
		Error error;
		if (!mixer_open(ao->mixer, error))
			FormatWarning(output_domain,
				      "Failed to open mixer for '%s'",
				      ao->name);
	}

	return open;
}

/**
 * Same as audio_output_close(), but expects the lock to be held by
 * the caller.
 */
static void
audio_output_close_locked(struct audio_output *ao)
{
	assert(ao != nullptr);
	assert(ao->allow_play);

	if (ao->mixer != nullptr)
		mixer_auto_close(ao->mixer);

	assert(!ao->open || !ao->fail_timer.IsDefined());

	if (ao->open)
		ao_command(ao, AO_COMMAND_CLOSE);
	else
		ao->fail_timer.Reset();
}

bool
audio_output_update(struct audio_output *ao,
		    const AudioFormat audio_format,
		    const MusicPipe &mp)
{
	const ScopeLock protect(ao->mutex);

	if (ao->enabled && ao->really_enabled) {
		if (ao->fail_timer.Check(REOPEN_AFTER * 1000)) {
			return audio_output_open(ao, audio_format, mp);
		}
	} else if (audio_output_is_open(ao))
		audio_output_close_locked(ao);

	return false;
}

void
audio_output_play(struct audio_output *ao)
{
	const ScopeLock protect(ao->mutex);

	assert(ao->allow_play);

	if (audio_output_is_open(ao) && !ao->in_playback_loop &&
	    !ao->woken_for_play) {
		ao->woken_for_play = true;
		ao->cond.signal();
	}
}

void audio_output_pause(struct audio_output *ao)
{
	if (ao->mixer != nullptr && ao->plugin->pause == nullptr)
		/* the device has no pause mode: close the mixer,
		   unless its "global" flag is set (checked by
		   mixer_auto_close()) */
		mixer_auto_close(ao->mixer);

	const ScopeLock protect(ao->mutex);

	assert(ao->allow_play);
	if (audio_output_is_open(ao))
		ao_command_async(ao, AO_COMMAND_PAUSE);
}

void
audio_output_drain_async(struct audio_output *ao)
{
	const ScopeLock protect(ao->mutex);

	assert(ao->allow_play);
	if (audio_output_is_open(ao))
		ao_command_async(ao, AO_COMMAND_DRAIN);
}

void audio_output_cancel(struct audio_output *ao)
{
	const ScopeLock protect(ao->mutex);

	if (audio_output_is_open(ao)) {
		ao->allow_play = false;
		ao_command_async(ao, AO_COMMAND_CANCEL);
	}
}

void
audio_output_allow_play(struct audio_output *ao)
{
	const ScopeLock protect(ao->mutex);

	ao->allow_play = true;
	if (audio_output_is_open(ao))
		ao->cond.signal();
}

void
audio_output_release(struct audio_output *ao)
{
	if (ao->always_on)
		audio_output_pause(ao);
	else
		audio_output_close(ao);
}

void audio_output_close(struct audio_output *ao)
{
	assert(ao != nullptr);
	assert(!ao->open || !ao->fail_timer.IsDefined());

	const ScopeLock protect(ao->mutex);
	audio_output_close_locked(ao);
}

void audio_output_finish(struct audio_output *ao)
{
	audio_output_close(ao);

	assert(!ao->fail_timer.IsDefined());

	if (ao->thread.IsDefined()) {
		assert(ao->allow_play);
		ao_lock_command(ao, AO_COMMAND_KILL);
		ao->thread.Join();
	}

	audio_output_free(ao);
}
