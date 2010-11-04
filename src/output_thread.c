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

#include "output_thread.h"
#include "output_api.h"
#include "output_internal.h"
#include "chunk.h"
#include "pipe.h"
#include "player_control.h"

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

	ao->pipe = NULL;

	g_mutex_lock(ao->mutex);
	ao->chunk = NULL;
	ao->open = false;
	g_mutex_unlock(ao->mutex);

	ao_plugin_close(ao->plugin, ao->data);
	pcm_convert_deinit(&ao->convert_state);

	g_debug("closed plugin=%s name=\"%s\"", ao->plugin->name, ao->name);
}

static bool
ao_play_chunk(struct audio_output *ao, const struct music_chunk *chunk)
{
	const char *data = chunk->data;
	size_t size = chunk->length;
	GError *error = NULL;

	assert(!music_chunk_is_empty(chunk));
	assert(music_chunk_check_format(chunk, &ao->in_audio_format));
	assert(size % audio_format_frame_size(&ao->in_audio_format) == 0);

	if (chunk->tag != NULL)
		ao_plugin_send_tag(ao->plugin, ao->data, chunk->tag);

	if (size == 0)
		return true;

	if (!audio_format_equals(&ao->in_audio_format,
				 &ao->out_audio_format)) {
		data = pcm_convert(&ao->convert_state,
				   &ao->in_audio_format, data, size,
				   &ao->out_audio_format, &size);

		/* under certain circumstances, pcm_convert() may
		   return an empty buffer - this condition should be
		   investigated further, but for now, do this check as
		   a workaround: */
		if (data == NULL)
			return true;
	}

	while (size > 0 && ao->command == AO_COMMAND_NONE) {
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
			g_mutex_lock(ao->mutex);

			assert(ao->fail_timer == NULL);
			ao->fail_timer = g_timer_new();

			g_mutex_unlock(ao->mutex);
			return false;
		}

		assert(nbytes <= size);
		assert(nbytes % audio_format_frame_size(&ao->out_audio_format) == 0);

		data += nbytes;
		size -= nbytes;
	}

	return true;
}

static void ao_play(struct audio_output *ao)
{
	bool success;
	const struct music_chunk *chunk;

	assert(ao->pipe != NULL);

	g_mutex_lock(ao->mutex);
	chunk = ao->chunk;
	if (chunk != NULL)
		/* continue the previous play() call */
		chunk = chunk->next;
	else
		chunk = music_pipe_peek(ao->pipe);
	ao->chunk_finished = false;

	while (chunk != NULL && ao->command == AO_COMMAND_NONE) {
		assert(!ao->chunk_finished);

		ao->chunk = chunk;
		g_mutex_unlock(ao->mutex);

		success = ao_play_chunk(ao, chunk);

		g_mutex_lock(ao->mutex);

		if (!success) {
			assert(ao->chunk == NULL);
			break;
		}

		assert(ao->chunk == chunk);
		chunk = chunk->next;
	}

	ao->chunk_finished = true;
	g_mutex_unlock(ao->mutex);

	notify_signal(&pc.notify);
}

static void ao_pause(struct audio_output *ao)
{
	bool ret;

	ao_plugin_cancel(ao->plugin, ao->data);
	ao->pause = true;
	ao_command_finished(ao);

	do {
		ret = ao_plugin_pause(ao->plugin, ao->data);
		if (!ret) {
			ao_close(ao);
			break;
		}
	} while (ao->command == AO_COMMAND_NONE);

	ao->pause = false;
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
			assert(ao->pipe != NULL);
			assert(ao->chunk == NULL);

			error = NULL;
			ret = ao_plugin_open(ao->plugin, ao->data,
					     &ao->out_audio_format,
					     &error);

			assert(!ao->open);
			if (ret) {
				pcm_convert_init(&ao->convert_state);

				g_mutex_lock(ao->mutex);
				ao->open = true;
				g_mutex_unlock(ao->mutex);

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
			assert(ao->pipe != NULL);

			ao->pipe = NULL;
			ao->chunk = NULL;

			ao_plugin_cancel(ao->plugin, ao->data);
			ao_close(ao);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_PAUSE:
			if (!ao->open) {
				/* the output has failed after
				   audio_output_all_pause() has
				   submitted the PAUSE command; bail
				   out */
				ao_command_finished(ao);
				break;
			}

			ao_pause(ao);
			/* don't "break" here: this might cause
			   ao_play() to be called when command==CLOSE
			   ends the paused state - "continue" checks
			   the new command first */
			continue;

		case AO_COMMAND_CANCEL:
			ao->chunk = NULL;
			if (ao->open)
				ao_plugin_cancel(ao->plugin, ao->data);

			/* we must clear the notification now, because
			   the notify_wait() call below must wait
			   until audio_output_all_cancel() has cleared
			   the pipe; if another notification happens
			   to be still pending, we get a race
			   condition with a crash or an assertion
			   failure */
			notify_clear(&ao->notify);

			ao_command_finished(ao);

			/* the player thread will now clear our music
			   pipe - wait for a notify, to give it some
			   time */
			notify_wait(&ao->notify);
			continue;

		case AO_COMMAND_KILL:
			ao->chunk = NULL;
			ao_command_finished(ao);
			return NULL;
		}

		if (ao->open)
			ao_play(ao);

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
