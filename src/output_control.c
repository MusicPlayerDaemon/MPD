/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "output_control.h"
#include "output_api.h"
#include "output_internal.h"
#include "output_thread.h"

#include <assert.h>
#include <stdlib.h>

enum {
	/** after a failure, wait this number of seconds before
	    automatically reopening the device */
	REOPEN_AFTER = 10,
};

struct notify audio_output_client_notify;

static void ao_command_wait(struct audio_output *ao)
{
	while (ao->command != AO_COMMAND_NONE) {
		notify_signal(&ao->notify);
		notify_wait(&audio_output_client_notify);
	}
}

static void ao_command(struct audio_output *ao, enum audio_output_command cmd)
{
	assert(ao->command == AO_COMMAND_NONE);
	ao->command = cmd;
	ao_command_wait(ao);
}

static void ao_command_async(struct audio_output *ao,
			     enum audio_output_command cmd)
{
	assert(ao->command == AO_COMMAND_NONE);
	ao->command = cmd;
	notify_signal(&ao->notify);
}

static bool
audio_output_open(struct audio_output *ao,
		  const struct audio_format *audio_format,
		  const struct music_pipe *mp)
{
	assert(mp != NULL);

	if (ao->fail_timer != NULL) {
		g_timer_destroy(ao->fail_timer);
		ao->fail_timer = NULL;
	}

	if (ao->open &&
	    audio_format_equals(audio_format, &ao->in_audio_format)) {
		assert(ao->pipe == mp);

		return true;
	}

	ao->in_audio_format = *audio_format;
	ao->chunk = NULL;

	if (!ao->config_audio_format) {
		if (ao->open)
			audio_output_close(ao);

		/* no audio format is configured: copy in->out, let
		   the output's open() method determine the effective
		   out_audio_format */
		ao->out_audio_format = ao->in_audio_format;
	}

	ao->pipe = mp;

	if (ao->thread == NULL)
		audio_output_thread_start(ao);

	if (!ao->open)
		ao_command(ao, AO_COMMAND_OPEN);

	return ao->open;
}

bool
audio_output_update(struct audio_output *ao,
		    const struct audio_format *audio_format,
		    const struct music_pipe *mp)
{
	assert(mp != NULL);

	if (ao->enabled) {
		if (ao->fail_timer == NULL ||
		    g_timer_elapsed(ao->fail_timer, NULL) > REOPEN_AFTER)
			return audio_output_open(ao, audio_format, mp);
	} else if (audio_output_is_open(ao))
		audio_output_close(ao);

	return false;
}

void
audio_output_play(struct audio_output *ao)
{
	if (!ao->open)
		return;

	notify_signal(&ao->notify);
}

void audio_output_pause(struct audio_output *ao)
{
	ao_command_async(ao, AO_COMMAND_PAUSE);
}

void audio_output_cancel(struct audio_output *ao)
{
	ao_command_async(ao, AO_COMMAND_CANCEL);
}

void audio_output_close(struct audio_output *ao)
{
	assert(!ao->open || ao->fail_timer == NULL);

	if (ao->open)
		ao_command(ao, AO_COMMAND_CLOSE);
	else if (ao->fail_timer != NULL) {
		g_timer_destroy(ao->fail_timer);
		ao->fail_timer = NULL;
	}
}

void audio_output_finish(struct audio_output *ao)
{
	audio_output_close(ao);

	assert(ao->fail_timer == NULL);

	if (ao->thread != NULL) {
		ao_command(ao, AO_COMMAND_KILL);
		g_thread_join(ao->thread);
	}

	ao_plugin_finish(ao->plugin, ao->data);

	notify_deinit(&ao->notify);
	g_mutex_free(ao->mutex);
}
