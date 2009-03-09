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

	g_debug("closed plugin=%s name=\"%s\"", ao->plugin->name, ao->name);
}

static void ao_play(struct audio_output *ao)
{
	const char *data = ao->args.play.data;
	size_t size = ao->args.play.size;
	GError *error = NULL;

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

		nbytes = ao_plugin_play(ao->plugin, ao->data, data, size,
					&error);
		if (nbytes == 0) {
			/* play()==0 means failure */
			g_warning("\"%s\" [%s] failed to play: %s",
				  ao->name, ao->plugin->name, error->message);
			g_error_free(error);

			ao_plugin_cancel(ao->plugin, ao->data);
			ao_close(ao);

			/* don't automatically reopen this device for
			   10 seconds */
			ao->fail_timer = g_timer_new();
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
	GError *error;

	while (1) {
		switch (ao->command) {
		case AO_COMMAND_NONE:
			break;

		case AO_COMMAND_OPEN:
			assert(!ao->open);
			assert(ao->fail_timer == NULL);

			error = NULL;
			ret = ao_plugin_open(ao->plugin, ao->data,
					     &ao->out_audio_format,
					     &error);

			assert(!ao->open);
			if (ret) {
				pcm_convert_init(&ao->convert_state);
				ao->open = true;


				g_debug("opened plugin=%s name=\"%s\" "
					"audio_format=%u:%u:%u",
					ao->plugin->name,
					ao->name,
					ao->out_audio_format.sample_rate,
					ao->out_audio_format.bits,
					ao->out_audio_format.channels);

				if (!audio_format_equals(&ao->in_audio_format,
							 &ao->out_audio_format))
					g_debug("converting from %u:%u:%u",
						ao->in_audio_format.sample_rate,
						ao->in_audio_format.bits,
						ao->in_audio_format.channels);
			} else {
				g_warning("Failed to open \"%s\" [%s]: %s",
					  ao->name, ao->plugin->name,
					  error->message);
				g_error_free(error);

				ao->fail_timer = g_timer_new();
			}

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
