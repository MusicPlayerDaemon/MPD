/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
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

#include "output_thread.h"
#include "output_api.h"
#include "output_internal.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "output"

enum {
	/** after a failure, wait this number of seconds before
	    automatically reopening the device */
	REOPEN_AFTER = 10,
};

static void ao_command_finished(struct audio_output *ao)
{
	assert(ao->command != AO_COMMAND_NONE);
	ao->command = AO_COMMAND_NONE;
	notify_signal(&audio_output_client_notify);
}

static void
ao_close(struct audio_output *ao)
{
	assert(ao->open);

	ao_plugin_close(ao->plugin, ao->data);
	pcm_convert_deinit(&ao->convert_state);
	ao->open = false;
}

static void ao_play(struct audio_output *ao)
{
	const char *data = ao->args.play.data;
	size_t size = ao->args.play.size;

	assert(size > 0);
	assert(size % audio_format_frame_size(&ao->in_audio_format) == 0);

	if (!audio_format_equals(&ao->in_audio_format, &ao->out_audio_format)) {
		data = pcm_convert(&ao->convert_state,
				   &ao->in_audio_format, data, size,
				   &ao->out_audio_format, &size);

		/* under certain circumstances, pcm_convert() may
		   return an empty buffer - this condition should be
		   investigated further, but for now, do this check as
		   a workaround: */
		if (data == NULL)
			return;
	}

	while (size > 0) {
		size_t nbytes;

		nbytes = ao_plugin_play(ao->plugin, ao->data, data, size);
		if (nbytes == 0) {
			/* play()==0 means failure */
			ao_plugin_cancel(ao->plugin, ao->data);
			ao_close(ao);
			break;
		}

		assert(nbytes <= size);
		assert(nbytes % audio_format_frame_size(&ao->out_audio_format) == 0);

		data += nbytes;
		size -= nbytes;
	}

	ao_command_finished(ao);
}

static void ao_pause(struct audio_output *ao)
{
	bool ret;

	ao_plugin_cancel(ao->plugin, ao->data);
	ao_command_finished(ao);

	do {
		ret = ao_plugin_pause(ao->plugin, ao->data);
		if (!ret) {
			ao_close(ao);
			break;
		}
	} while (ao->command == AO_COMMAND_NONE);
}

static gpointer audio_output_task(gpointer arg)
{
	struct audio_output *ao = arg;
	bool ret;

	while (1) {
		switch (ao->command) {
		case AO_COMMAND_NONE:
			break;

		case AO_COMMAND_OPEN:
			assert(!ao->open);

			ret = ao_plugin_open(ao->plugin, ao->data,
					     &ao->out_audio_format);

			assert(!ao->open);
			if (ret) {
				pcm_convert_init(&ao->convert_state);
				ao->open = true;
			} else
				ao->reopen_after = time(NULL) + REOPEN_AFTER;

			ao_command_finished(ao);
			break;

		case AO_COMMAND_CLOSE:
			assert(ao->open);

			ao_plugin_cancel(ao->plugin, ao->data);
			ao_close(ao);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_PLAY:
			ao_play(ao);
			break;

		case AO_COMMAND_PAUSE:
			ao_pause(ao);
			break;

		case AO_COMMAND_CANCEL:
			ao_plugin_cancel(ao->plugin, ao->data);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_SEND_TAG:
			ao_plugin_send_tag(ao->plugin, ao->data, ao->args.tag);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_KILL:
			ao_command_finished(ao);
			return NULL;
		}

		notify_wait(&ao->notify);
	}
}

void audio_output_thread_start(struct audio_output *ao)
{
	GError *e = NULL;

	assert(ao->command == AO_COMMAND_NONE);

	if (!(ao->thread = g_thread_create(audio_output_task, ao, true, &e)))
		g_error("Failed to spawn output task: %s\n", e->message);
}
