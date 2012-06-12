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
#include "output_thread.h"
#include "output_api.h"
#include "output_internal.h"
#include "chunk.h"
#include "pipe.h"
#include "player_control.h"
#include "pcm_mix.h"
#include "filter_plugin.h"
#include "filter/convert_filter_plugin.h"
#include "filter/replay_gain_filter_plugin.h"
#include "mpd_error.h"
#include "notify.h"
#include "gcc.h"

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

	g_mutex_unlock(ao->mutex);
	notify_signal(&audio_output_client_notify);
	g_mutex_lock(ao->mutex);
}

static bool
ao_enable(struct audio_output *ao)
{
	GError *error = NULL;
	bool success;

	if (ao->really_enabled)
		return true;

	g_mutex_unlock(ao->mutex);
	success = ao_plugin_enable(ao, &error);
	g_mutex_lock(ao->mutex);
	if (!success) {
		g_warning("Failed to enable \"%s\" [%s]: %s\n",
			  ao->name, ao->plugin->name, error->message);
		g_error_free(error);
		return false;
	}

	ao->really_enabled = true;
	return true;
}

static void
ao_close(struct audio_output *ao, bool drain);

static void
ao_disable(struct audio_output *ao)
{
	if (ao->open)
		ao_close(ao, false);

	if (ao->really_enabled) {
		ao->really_enabled = false;

		g_mutex_unlock(ao->mutex);
		ao_plugin_disable(ao);
		g_mutex_lock(ao->mutex);
	}
}

static const struct audio_format *
ao_filter_open(struct audio_output *ao,
	       struct audio_format *audio_format,
	       GError **error_r)
{
	assert(audio_format_valid(audio_format));

	/* the replay_gain filter cannot fail here */
	if (ao->replay_gain_filter != NULL)
		filter_open(ao->replay_gain_filter, audio_format, error_r);
	if (ao->other_replay_gain_filter != NULL)
		filter_open(ao->other_replay_gain_filter, audio_format,
			    error_r);

	const struct audio_format *af
		= filter_open(ao->filter, audio_format, error_r);
	if (af == NULL) {
		if (ao->replay_gain_filter != NULL)
			filter_close(ao->replay_gain_filter);
		if (ao->other_replay_gain_filter != NULL)
			filter_close(ao->other_replay_gain_filter);
	}

	return af;
}

static void
ao_filter_close(struct audio_output *ao)
{
	if (ao->replay_gain_filter != NULL)
		filter_close(ao->replay_gain_filter);
	if (ao->other_replay_gain_filter != NULL)
		filter_close(ao->other_replay_gain_filter);

	filter_close(ao->filter);
}

static void
ao_open(struct audio_output *ao)
{
	bool success;
	GError *error = NULL;
	const struct audio_format *filter_audio_format;
	struct audio_format_string af_string;

	assert(!ao->open);
	assert(ao->pipe != NULL);
	assert(ao->chunk == NULL);
	assert(audio_format_valid(&ao->in_audio_format));

	if (ao->fail_timer != NULL) {
		/* this can only happen when this
		   output thread fails while
		   audio_output_open() is run in the
		   player thread */
		g_timer_destroy(ao->fail_timer);
		ao->fail_timer = NULL;
	}

	/* enable the device (just in case the last enable has failed) */

	if (!ao_enable(ao))
		/* still no luck */
		return;

	/* open the filter */

	filter_audio_format = ao_filter_open(ao, &ao->in_audio_format, &error);
	if (filter_audio_format == NULL) {
		g_warning("Failed to open filter for \"%s\" [%s]: %s",
			  ao->name, ao->plugin->name, error->message);
		g_error_free(error);

		ao->fail_timer = g_timer_new();
		return;
	}

	assert(audio_format_valid(filter_audio_format));

	ao->out_audio_format = *filter_audio_format;
	audio_format_mask_apply(&ao->out_audio_format,
				&ao->config_audio_format);

	g_mutex_unlock(ao->mutex);
	success = ao_plugin_open(ao, &ao->out_audio_format, &error);
	g_mutex_lock(ao->mutex);

	assert(!ao->open);

	if (!success) {
		g_warning("Failed to open \"%s\" [%s]: %s",
			  ao->name, ao->plugin->name, error->message);
		g_error_free(error);

		ao_filter_close(ao);
		ao->fail_timer = g_timer_new();
		return;
	}

	convert_filter_set(ao->convert_filter, &ao->out_audio_format);

	ao->open = true;

	g_debug("opened plugin=%s name=\"%s\" "
		"audio_format=%s",
		ao->plugin->name, ao->name,
		audio_format_to_string(&ao->out_audio_format, &af_string));

	if (!audio_format_equals(&ao->in_audio_format,
				 &ao->out_audio_format))
		g_debug("converting from %s",
			audio_format_to_string(&ao->in_audio_format,
					       &af_string));
}

static void
ao_close(struct audio_output *ao, bool drain)
{
	assert(ao->open);

	ao->pipe = NULL;

	ao->chunk = NULL;
	ao->open = false;

	g_mutex_unlock(ao->mutex);

	if (drain)
		ao_plugin_drain(ao);
	else
		ao_plugin_cancel(ao);

	ao_plugin_close(ao);
	ao_filter_close(ao);

	g_mutex_lock(ao->mutex);

	g_debug("closed plugin=%s name=\"%s\"", ao->plugin->name, ao->name);
}

static void
ao_reopen_filter(struct audio_output *ao)
{
	const struct audio_format *filter_audio_format;
	GError *error = NULL;

	ao_filter_close(ao);
	filter_audio_format = ao_filter_open(ao, &ao->in_audio_format, &error);
	if (filter_audio_format == NULL) {
		g_warning("Failed to open filter for \"%s\" [%s]: %s",
			  ao->name, ao->plugin->name, error->message);
		g_error_free(error);

		/* this is a little code duplication fro ao_close(),
		   but we cannot call this function because we must
		   not call filter_close(ao->filter) again */

		ao->pipe = NULL;

		ao->chunk = NULL;
		ao->open = false;
		ao->fail_timer = g_timer_new();

		g_mutex_unlock(ao->mutex);
		ao_plugin_close(ao);
		g_mutex_lock(ao->mutex);

		return;
	}

	convert_filter_set(ao->convert_filter, &ao->out_audio_format);
}

static void
ao_reopen(struct audio_output *ao)
{
	if (!audio_format_fully_defined(&ao->config_audio_format)) {
		if (ao->open) {
			const struct music_pipe *mp = ao->pipe;
			ao_close(ao, true);
			ao->pipe = mp;
		}

		/* no audio format is configured: copy in->out, let
		   the output's open() method determine the effective
		   out_audio_format */
		ao->out_audio_format = ao->in_audio_format;
		audio_format_mask_apply(&ao->out_audio_format,
					&ao->config_audio_format);
	}

	if (ao->open)
		/* the audio format has changed, and all filters have
		   to be reconfigured */
		ao_reopen_filter(ao);
	else
		ao_open(ao);
}

/**
 * Wait until the output's delay reaches zero.
 *
 * @return true if playback should be continued, false if a command
 * was issued
 */
static bool
ao_wait(struct audio_output *ao)
{
	while (true) {
		unsigned delay = ao_plugin_delay(ao);
		if (delay == 0)
			return true;

		GTimeVal tv;
		g_get_current_time(&tv);
		g_time_val_add(&tv, delay * 1000);
		(void)g_cond_timed_wait(ao->cond, ao->mutex, &tv);

		if (ao->command != AO_COMMAND_NONE)
			return false;
	}
}

static const char *
ao_chunk_data(struct audio_output *ao, const struct music_chunk *chunk,
	      struct filter *replay_gain_filter,
	      unsigned *replay_gain_serial_p,
	      size_t *length_r)
{
	assert(chunk != NULL);
	assert(!music_chunk_is_empty(chunk));
	assert(music_chunk_check_format(chunk, &ao->in_audio_format));

	const char *data = chunk->data;
	size_t length = chunk->length;

	(void)ao;

	assert(length % audio_format_frame_size(&ao->in_audio_format) == 0);

	if (length > 0 && replay_gain_filter != NULL) {
		if (chunk->replay_gain_serial != *replay_gain_serial_p) {
			replay_gain_filter_set_info(replay_gain_filter,
						    chunk->replay_gain_serial != 0
						    ? &chunk->replay_gain_info
						    : NULL);
			*replay_gain_serial_p = chunk->replay_gain_serial;
		}

		GError *error = NULL;
		data = filter_filter(replay_gain_filter, data, length,
				     &length, &error);
		if (data == NULL) {
			g_warning("\"%s\" [%s] failed to filter: %s",
				  ao->name, ao->plugin->name, error->message);
			g_error_free(error);
			return NULL;
		}
	}

	*length_r = length;
	return data;
}

static const char *
ao_filter_chunk(struct audio_output *ao, const struct music_chunk *chunk,
		size_t *length_r)
{
	GError *error = NULL;

	size_t length;
	const char *data = ao_chunk_data(ao, chunk, ao->replay_gain_filter,
					 &ao->replay_gain_serial, &length);
	if (data == NULL)
		return NULL;

	if (length == 0) {
		/* empty chunk, nothing to do */
		*length_r = 0;
		return data;
	}

	/* cross-fade */

	if (chunk->other != NULL) {
		size_t other_length;
		const char *other_data =
			ao_chunk_data(ao, chunk->other,
				      ao->other_replay_gain_filter,
				      &ao->other_replay_gain_serial,
				      &other_length);
		if (other_data == NULL)
			return NULL;

		if (other_length == 0) {
			*length_r = 0;
			return data;
		}

		/* if the "other" chunk is longer, then that trailer
		   is used as-is, without mixing; it is part of the
		   "next" song being faded in, and if there's a rest,
		   it means cross-fading ends here */

		if (length > other_length)
			length = other_length;

		char *dest = pcm_buffer_get(&ao->cross_fade_buffer,
					    other_length);
		memcpy(dest, other_data, other_length);
		if (!pcm_mix(dest, data, length, ao->in_audio_format.format,
			     1.0 - chunk->mix_ratio)) {
			g_warning("Cannot cross-fade format %s",
				  sample_format_to_string(ao->in_audio_format.format));
			return NULL;
		}

		data = dest;
		length = other_length;
	}

	/* apply filter chain */

	data = filter_filter(ao->filter, data, length, &length, &error);
	if (data == NULL) {
		g_warning("\"%s\" [%s] failed to filter: %s",
			  ao->name, ao->plugin->name, error->message);
		g_error_free(error);
		return NULL;
	}

	*length_r = length;
	return data;
}

static bool
ao_play_chunk(struct audio_output *ao, const struct music_chunk *chunk)
{
	GError *error = NULL;

	assert(ao != NULL);
	assert(ao->filter != NULL);

	if (chunk->tag != NULL) {
		g_mutex_unlock(ao->mutex);
		ao_plugin_send_tag(ao, chunk->tag);
		g_mutex_lock(ao->mutex);
	}

	size_t size;
#if GCC_CHECK_VERSION(4,7)
	/* workaround -Wmaybe-uninitialized false positive */
	size = 0;
#endif
	const char *data = ao_filter_chunk(ao, chunk, &size);
	if (data == NULL) {
		ao_close(ao, false);

		/* don't automatically reopen this device for 10
		   seconds */
		ao->fail_timer = g_timer_new();
		return false;
	}

	while (size > 0 && ao->command == AO_COMMAND_NONE) {
		size_t nbytes;

		if (!ao_wait(ao))
			break;

		g_mutex_unlock(ao->mutex);
		nbytes = ao_plugin_play(ao, data, size, &error);
		g_mutex_lock(ao->mutex);
		if (nbytes == 0) {
			/* play()==0 means failure */
			g_warning("\"%s\" [%s] failed to play: %s",
				  ao->name, ao->plugin->name, error->message);
			g_error_free(error);

			ao_close(ao, false);

			/* don't automatically reopen this device for
			   10 seconds */
			assert(ao->fail_timer == NULL);
			ao->fail_timer = g_timer_new();

			return false;
		}

		assert(nbytes <= size);
		assert(nbytes % audio_format_frame_size(&ao->out_audio_format) == 0);

		data += nbytes;
		size -= nbytes;
	}

	return true;
}

static const struct music_chunk *
ao_next_chunk(struct audio_output *ao)
{
	return ao->chunk != NULL
		/* continue the previous play() call */
		? ao->chunk->next
		/* get the first chunk from the pipe */
		: music_pipe_peek(ao->pipe);
}

/**
 * Plays all remaining chunks, until the tail of the pipe has been
 * reached (and no more chunks are queued), or until a command is
 * received.
 *
 * @return true if at least one chunk has been available, false if the
 * tail of the pipe was already reached
 */
static bool
ao_play(struct audio_output *ao)
{
	bool success;
	const struct music_chunk *chunk;

	assert(ao->pipe != NULL);

	chunk = ao_next_chunk(ao);
	if (chunk == NULL)
		/* no chunk available */
		return false;

	ao->chunk_finished = false;

	while (chunk != NULL && ao->command == AO_COMMAND_NONE) {
		assert(!ao->chunk_finished);

		ao->chunk = chunk;

		success = ao_play_chunk(ao, chunk);
		if (!success) {
			assert(ao->chunk == NULL);
			break;
		}

		assert(ao->chunk == chunk);
		chunk = chunk->next;
	}

	ao->chunk_finished = true;

	g_mutex_unlock(ao->mutex);
	player_lock_signal(ao->player_control);
	g_mutex_lock(ao->mutex);

	return true;
}

static void ao_pause(struct audio_output *ao)
{
	bool ret;

	g_mutex_unlock(ao->mutex);
	ao_plugin_cancel(ao);
	g_mutex_lock(ao->mutex);

	ao->pause = true;
	ao_command_finished(ao);

	do {
		if (!ao_wait(ao))
			break;

		g_mutex_unlock(ao->mutex);
		ret = ao_plugin_pause(ao);
		g_mutex_lock(ao->mutex);

		if (!ret) {
			ao_close(ao, false);
			break;
		}
	} while (ao->command == AO_COMMAND_NONE);

	ao->pause = false;
}

static gpointer audio_output_task(gpointer arg)
{
	struct audio_output *ao = arg;

	g_mutex_lock(ao->mutex);

	while (1) {
		switch (ao->command) {
		case AO_COMMAND_NONE:
			break;

		case AO_COMMAND_ENABLE:
			ao_enable(ao);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_DISABLE:
			ao_disable(ao);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_OPEN:
			ao_open(ao);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_REOPEN:
			ao_reopen(ao);
			ao_command_finished(ao);
			break;

		case AO_COMMAND_CLOSE:
			assert(ao->open);
			assert(ao->pipe != NULL);

			ao_close(ao, false);
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

		case AO_COMMAND_DRAIN:
			if (ao->open) {
				assert(ao->chunk == NULL);
				assert(music_pipe_peek(ao->pipe) == NULL);

				g_mutex_unlock(ao->mutex);
				ao_plugin_drain(ao);
				g_mutex_lock(ao->mutex);
			}

			ao_command_finished(ao);
			continue;

		case AO_COMMAND_CANCEL:
			ao->chunk = NULL;

			if (ao->open) {
				g_mutex_unlock(ao->mutex);
				ao_plugin_cancel(ao);
				g_mutex_lock(ao->mutex);
			}

			ao_command_finished(ao);
			continue;

		case AO_COMMAND_KILL:
			ao->chunk = NULL;
			ao_command_finished(ao);
			g_mutex_unlock(ao->mutex);
			return NULL;
		}

		if (ao->open && ao->allow_play && ao_play(ao))
			/* don't wait for an event if there are more
			   chunks in the pipe */
			continue;

		if (ao->command == AO_COMMAND_NONE)
			g_cond_wait(ao->cond, ao->mutex);
	}
}

void audio_output_thread_start(struct audio_output *ao)
{
	GError *e = NULL;

	assert(ao->command == AO_COMMAND_NONE);

	if (!(ao->thread = g_thread_create(audio_output_task, ao, true, &e)))
		MPD_ERROR("Failed to spawn output task: %s\n", e->message);
}
