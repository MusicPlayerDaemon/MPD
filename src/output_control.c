/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "output_control.h"
#include "output_api.h"
#include "output_internal.h"
#include "output_thread.h"
#include "mixer_control.h"
#include "mixer_plugin.h"
#include "filter_plugin.h"
#include "notify.h"

#include <assert.h>
#include <stdlib.h>

enum {
	/** after a failure, wait this number of seconds before
	    automatically reopening the device */
	REOPEN_AFTER = 10,
};

struct notify audio_output_client_notify;

/**
 * Waits for command completion.
 *
 * @param ao the #audio_output instance; must be locked
 */
static void ao_command_wait(struct audio_output *ao)
{
	while (ao->command != AO_COMMAND_NONE) {
		g_mutex_unlock(ao->mutex);
		notify_wait(&audio_output_client_notify);
		g_mutex_lock(ao->mutex);
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
	g_cond_signal(ao->cond);
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
	g_mutex_lock(ao->mutex);
	ao_command(ao, cmd);
	g_mutex_unlock(ao->mutex);
}

void
audio_output_enable(struct audio_output *ao)
{
	if (ao->thread == NULL) {
		if (ao->plugin->enable == NULL) {
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
	if (ao->thread == NULL) {
		if (ao->plugin->disable == NULL)
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
		  const struct audio_format *audio_format,
		  const struct music_pipe *mp)
{
	bool open;

	assert(ao != NULL);
	assert(ao->allow_play);
	assert(audio_format_valid(audio_format));
	assert(mp != NULL);

	if (ao->fail_timer != NULL) {
		g_timer_destroy(ao->fail_timer);
		ao->fail_timer = NULL;
	}

	if (ao->open &&
	    audio_format_equals(audio_format, &ao->in_audio_format)) {
		assert(ao->pipe == mp ||
		       (ao->always_on && ao->pause));

		if (ao->pause) {
			ao->chunk = NULL;
			ao->pipe = mp;

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

	ao->in_audio_format = *audio_format;
	ao->chunk = NULL;

	ao->pipe = mp;

	if (ao->thread == NULL)
		audio_output_thread_start(ao);

	ao_command(ao, ao->open ? AO_COMMAND_REOPEN : AO_COMMAND_OPEN);
	open = ao->open;

	if (open && ao->mixer != NULL) {
		GError *error = NULL;

		if (!mixer_open(ao->mixer, &error)) {
			g_warning("Failed to open mixer for '%s': %s",
				  ao->name, error->message);
			g_error_free(error);
		}
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
	assert(ao != NULL);
	assert(ao->allow_play);

	if (ao->mixer != NULL)
		mixer_auto_close(ao->mixer);

	assert(!ao->open || ao->fail_timer == NULL);

	if (ao->open)
		ao_command(ao, AO_COMMAND_CLOSE);
	else if (ao->fail_timer != NULL) {
		g_timer_destroy(ao->fail_timer);
		ao->fail_timer = NULL;
	}
}

bool
audio_output_update(struct audio_output *ao,
		    const struct audio_format *audio_format,
		    const struct music_pipe *mp)
{
	assert(mp != NULL);

	g_mutex_lock(ao->mutex);

	if (ao->enabled && ao->really_enabled) {
		if (ao->fail_timer == NULL ||
		    g_timer_elapsed(ao->fail_timer, NULL) > REOPEN_AFTER) {
			bool success = audio_output_open(ao, audio_format, mp);
			g_mutex_unlock(ao->mutex);
			return success;
		}
	} else if (audio_output_is_open(ao))
		audio_output_close_locked(ao);

	g_mutex_unlock(ao->mutex);
	return false;
}

void
audio_output_play(struct audio_output *ao)
{
	g_mutex_lock(ao->mutex);

	assert(ao->allow_play);

	if (audio_output_is_open(ao))
		g_cond_signal(ao->cond);

	g_mutex_unlock(ao->mutex);
}

void audio_output_pause(struct audio_output *ao)
{
	if (ao->mixer != NULL && ao->plugin->pause == NULL)
		/* the device has no pause mode: close the mixer,
		   unless its "global" flag is set (checked by
		   mixer_auto_close()) */
		mixer_auto_close(ao->mixer);

	g_mutex_lock(ao->mutex);
	assert(ao->allow_play);
	if (audio_output_is_open(ao))
		ao_command_async(ao, AO_COMMAND_PAUSE);
	g_mutex_unlock(ao->mutex);
}

void
audio_output_drain_async(struct audio_output *ao)
{
	g_mutex_lock(ao->mutex);
	assert(ao->allow_play);
	if (audio_output_is_open(ao))
		ao_command_async(ao, AO_COMMAND_DRAIN);
	g_mutex_unlock(ao->mutex);
}

void audio_output_cancel(struct audio_output *ao)
{
	g_mutex_lock(ao->mutex);

	if (audio_output_is_open(ao)) {
		ao->allow_play = false;
		ao_command_async(ao, AO_COMMAND_CANCEL);
	}

	g_mutex_unlock(ao->mutex);
}

void
audio_output_allow_play(struct audio_output *ao)
{
	g_mutex_lock(ao->mutex);

	ao->allow_play = true;
	if (audio_output_is_open(ao))
		g_cond_signal(ao->cond);

	g_mutex_unlock(ao->mutex);
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
	assert(ao != NULL);
	assert(!ao->open || ao->fail_timer == NULL);

	g_mutex_lock(ao->mutex);
	audio_output_close_locked(ao);
	g_mutex_unlock(ao->mutex);
}

void audio_output_finish(struct audio_output *ao)
{
	audio_output_close(ao);

	assert(ao->fail_timer == NULL);

	if (ao->thread != NULL) {
		assert(ao->allow_play);
		ao_lock_command(ao, AO_COMMAND_KILL);
		g_thread_join(ao->thread);
		ao->thread = NULL;
	}

	audio_output_free(ao);
}
