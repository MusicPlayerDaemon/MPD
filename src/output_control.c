/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "output_control.h"
#include "output_api.h"
#include "output_internal.h"
#include "output_thread.h"

#include <assert.h>
#include <stdlib.h>

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

bool
audio_output_open(struct audio_output *ao,
		  const struct audio_format *audio_format)
{
	ao->reopen_after = 0;

	if (ao->open &&
	    audio_format_equals(audio_format, &ao->in_audio_format)) {
		return true;
	}

	ao->in_audio_format = *audio_format;

	if (audio_format_defined(&ao->config_audio_format)) {
		/* copy config_audio_format to out_audio_format only if the
		   device is not yet open; if it is already open,
		   plugin->open() may have modified out_audio_format,
		   and the value is already ok */
		if (!ao->open)
			ao->out_audio_format =
				ao->config_audio_format;
	} else {
		ao->out_audio_format = ao->in_audio_format;
		if (ao->open)
			audio_output_close(ao);
	}

	if (ao->thread == NULL)
		audio_output_thread_start(ao);

	if (!ao->open)
		ao_command(ao, AO_COMMAND_OPEN);

	return ao->open;
}

void
audio_output_update(struct audio_output *ao,
		    const struct audio_format *audio_format)
{
	if (ao->enabled) {
		if (ao->reopen_after == 0 || time(NULL) > ao->reopen_after)
			audio_output_open(ao, audio_format);
	} else if (audio_output_is_open(ao))
		audio_output_close(ao);
}

void
audio_output_signal(struct audio_output *ao)
{
	notify_signal(&ao->notify);
}

void
audio_output_play(struct audio_output *ao, const void *chunk, size_t size)
{
	assert(size > 0);

	if (!ao->open)
		return;

	ao->args.play.data = chunk;
	ao->args.play.size = size;
	ao_command_async(ao, AO_COMMAND_PLAY);
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
	if (ao->open)
		ao_command(ao, AO_COMMAND_CLOSE);
}

void audio_output_finish(struct audio_output *ao)
{
	audio_output_close(ao);

	if (ao->thread != NULL) {
		ao_command(ao, AO_COMMAND_KILL);
		g_thread_join(ao->thread);
	}

	ao_plugin_finish(ao->plugin, ao->data);

	notify_deinit(&ao->notify);
}

void
audio_output_send_tag(struct audio_output *ao, const struct tag *tag)
{
	if (ao->plugin->send_tag == NULL)
		return;

	ao->args.tag = tag;
	ao_command_async(ao, AO_COMMAND_SEND_TAG);
}
