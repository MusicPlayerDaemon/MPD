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
#include "OutputThread.hxx"
#include "OutputInternal.hxx"
#include "OutputAPI.hxx"
#include "OutputError.hxx"
#include "pcm/PcmMix.hxx"
#include "notify.hxx"
#include "FilterInternal.hxx"
#include "filter/ConvertFilterPlugin.hxx"
#include "filter/ReplayGainFilterPlugin.hxx"
#include "PlayerControl.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "system/FatalError.hxx"
#include "util/Error.hxx"
#include "Log.hxx"
#include "Compiler.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

static void ao_command_finished(struct audio_output *ao)
{
	assert(ao->command != AO_COMMAND_NONE);
	ao->command = AO_COMMAND_NONE;

	ao->mutex.unlock();
	audio_output_client_notify.Signal();
	ao->mutex.lock();
}

static bool
ao_enable(struct audio_output *ao)
{
	Error error;
	bool success;

	if (ao->really_enabled)
		return true;

	ao->mutex.unlock();
	success = ao_plugin_enable(ao, error);
	ao->mutex.lock();
	if (!success) {
		FormatError(error,
			    "Failed to enable \"%s\" [%s]",
			    ao->name, ao->plugin->name);
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

		ao->mutex.unlock();
		ao_plugin_disable(ao);
		ao->mutex.lock();
	}
}

static AudioFormat
ao_filter_open(struct audio_output *ao, AudioFormat &format,
	       Error &error_r)
{
	assert(format.IsValid());

	/* the replay_gain filter cannot fail here */
	if (ao->replay_gain_filter != nullptr)
		ao->replay_gain_filter->Open(format, error_r);
	if (ao->other_replay_gain_filter != nullptr)
		ao->other_replay_gain_filter->Open(format, error_r);

	const AudioFormat af = ao->filter->Open(format, error_r);
	if (!af.IsDefined()) {
		if (ao->replay_gain_filter != nullptr)
			ao->replay_gain_filter->Close();
		if (ao->other_replay_gain_filter != nullptr)
			ao->other_replay_gain_filter->Close();
	}

	return af;
}

static void
ao_filter_close(struct audio_output *ao)
{
	if (ao->replay_gain_filter != nullptr)
		ao->replay_gain_filter->Close();
	if (ao->other_replay_gain_filter != nullptr)
		ao->other_replay_gain_filter->Close();

	ao->filter->Close();
}

static void
ao_open(struct audio_output *ao)
{
	bool success;
	Error error;
	struct audio_format_string af_string;

	assert(!ao->open);
	assert(ao->pipe != nullptr);
	assert(ao->chunk == nullptr);
	assert(ao->in_audio_format.IsValid());

	if (ao->fail_timer != nullptr) {
		/* this can only happen when this
		   output thread fails while
		   audio_output_open() is run in the
		   player thread */
		g_timer_destroy(ao->fail_timer);
		ao->fail_timer = nullptr;
	}

	/* enable the device (just in case the last enable has failed) */

	if (!ao_enable(ao))
		/* still no luck */
		return;

	/* open the filter */

	const AudioFormat filter_audio_format =
		ao_filter_open(ao, ao->in_audio_format, error);
	if (!filter_audio_format.IsDefined()) {
		FormatError(error, "Failed to open filter for \"%s\" [%s]",
			    ao->name, ao->plugin->name);

		ao->fail_timer = g_timer_new();
		return;
	}

	assert(filter_audio_format.IsValid());

	ao->out_audio_format = filter_audio_format;
	ao->out_audio_format.ApplyMask(ao->config_audio_format);

	ao->mutex.unlock();
	success = ao_plugin_open(ao, ao->out_audio_format, error);
	ao->mutex.lock();

	assert(!ao->open);

	if (!success) {
		FormatError(error, "Failed to open \"%s\" [%s]",
			    ao->name, ao->plugin->name);

		ao_filter_close(ao);
		ao->fail_timer = g_timer_new();
		return;
	}

	convert_filter_set(ao->convert_filter, ao->out_audio_format);

	ao->open = true;

	FormatDebug(output_domain,
		    "opened plugin=%s name=\"%s\" audio_format=%s",
		    ao->plugin->name, ao->name,
		    audio_format_to_string(ao->out_audio_format, &af_string));

	if (ao->in_audio_format != ao->out_audio_format)
		FormatDebug(output_domain, "converting from %s",
			    audio_format_to_string(ao->in_audio_format,
						   &af_string));
}

static void
ao_close(struct audio_output *ao, bool drain)
{
	assert(ao->open);

	ao->pipe = nullptr;

	ao->chunk = nullptr;
	ao->open = false;

	ao->mutex.unlock();

	if (drain)
		ao_plugin_drain(ao);
	else
		ao_plugin_cancel(ao);

	ao_plugin_close(ao);
	ao_filter_close(ao);

	ao->mutex.lock();

	FormatDebug(output_domain, "closed plugin=%s name=\"%s\"",
		    ao->plugin->name, ao->name);
}

static void
ao_reopen_filter(struct audio_output *ao)
{
	Error error;

	ao_filter_close(ao);
	const AudioFormat filter_audio_format =
		ao_filter_open(ao, ao->in_audio_format, error);
	if (!filter_audio_format.IsDefined()) {
		FormatError(error,
			    "Failed to open filter for \"%s\" [%s]",
			    ao->name, ao->plugin->name);

		/* this is a little code duplication fro ao_close(),
		   but we cannot call this function because we must
		   not call filter_close(ao->filter) again */

		ao->pipe = nullptr;

		ao->chunk = nullptr;
		ao->open = false;
		ao->fail_timer = g_timer_new();

		ao->mutex.unlock();
		ao_plugin_close(ao);
		ao->mutex.lock();

		return;
	}

	convert_filter_set(ao->convert_filter, ao->out_audio_format);
}

static void
ao_reopen(struct audio_output *ao)
{
	if (!ao->config_audio_format.IsFullyDefined()) {
		if (ao->open) {
			const MusicPipe *mp = ao->pipe;
			ao_close(ao, true);
			ao->pipe = mp;
		}

		/* no audio format is configured: copy in->out, let
		   the output's open() method determine the effective
		   out_audio_format */
		ao->out_audio_format = ao->in_audio_format;
		ao->out_audio_format.ApplyMask(ao->config_audio_format);
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

		(void)ao->cond.timed_wait(ao->mutex, delay);

		if (ao->command != AO_COMMAND_NONE)
			return false;
	}
}

static const void *
ao_chunk_data(struct audio_output *ao, const struct music_chunk *chunk,
	      Filter *replay_gain_filter,
	      unsigned *replay_gain_serial_p,
	      size_t *length_r)
{
	assert(chunk != nullptr);
	assert(!chunk->IsEmpty());
	assert(chunk->CheckFormat(ao->in_audio_format));

	const void *data = chunk->data;
	size_t length = chunk->length;

	(void)ao;

	assert(length % ao->in_audio_format.GetFrameSize() == 0);

	if (length > 0 && replay_gain_filter != nullptr) {
		if (chunk->replay_gain_serial != *replay_gain_serial_p) {
			replay_gain_filter_set_info(replay_gain_filter,
						    chunk->replay_gain_serial != 0
						    ? &chunk->replay_gain_info
						    : nullptr);
			*replay_gain_serial_p = chunk->replay_gain_serial;
		}

		Error error;
		data = replay_gain_filter->FilterPCM(data, length,
						     &length, error);
		if (data == nullptr) {
			FormatError(error, "\"%s\" [%s] failed to filter",
				    ao->name, ao->plugin->name);
			return nullptr;
		}
	}

	*length_r = length;
	return data;
}

static const void *
ao_filter_chunk(struct audio_output *ao, const struct music_chunk *chunk,
		size_t *length_r)
{
	size_t length;
	const void *data = ao_chunk_data(ao, chunk, ao->replay_gain_filter,
					 &ao->replay_gain_serial, &length);
	if (data == nullptr)
		return nullptr;

	if (length == 0) {
		/* empty chunk, nothing to do */
		*length_r = 0;
		return data;
	}

	/* cross-fade */

	if (chunk->other != nullptr) {
		size_t other_length;
		const void *other_data =
			ao_chunk_data(ao, chunk->other,
				      ao->other_replay_gain_filter,
				      &ao->other_replay_gain_serial,
				      &other_length);
		if (other_data == nullptr)
			return nullptr;

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

		void *dest = ao->cross_fade_buffer.Get(other_length);
		memcpy(dest, other_data, other_length);
		if (!pcm_mix(dest, data, length,
			     ao->in_audio_format.format,
			     1.0 - chunk->mix_ratio)) {
			FormatError(output_domain,
				    "Cannot cross-fade format %s",
				    sample_format_to_string(ao->in_audio_format.format));
			return nullptr;
		}

		data = dest;
		length = other_length;
	}

	/* apply filter chain */

	Error error;
	data = ao->filter->FilterPCM(data, length, &length, error);
	if (data == nullptr) {
		FormatError(error, "\"%s\" [%s] failed to filter",
			    ao->name, ao->plugin->name);
		return nullptr;
	}

	*length_r = length;
	return data;
}

static bool
ao_play_chunk(struct audio_output *ao, const struct music_chunk *chunk)
{
	assert(ao != nullptr);
	assert(ao->filter != nullptr);

	if (ao->tags && gcc_unlikely(chunk->tag != nullptr)) {
		ao->mutex.unlock();
		ao_plugin_send_tag(ao, chunk->tag);
		ao->mutex.lock();
	}

	size_t size;
#if GCC_CHECK_VERSION(4,7)
	/* workaround -Wmaybe-uninitialized false positive */
	size = 0;
#endif
	const char *data = (const char *)ao_filter_chunk(ao, chunk, &size);
	if (data == nullptr) {
		ao_close(ao, false);

		/* don't automatically reopen this device for 10
		   seconds */
		ao->fail_timer = g_timer_new();
		return false;
	}

	Error error;

	while (size > 0 && ao->command == AO_COMMAND_NONE) {
		size_t nbytes;

		if (!ao_wait(ao))
			break;

		ao->mutex.unlock();
		nbytes = ao_plugin_play(ao, data, size, error);
		ao->mutex.lock();
		if (nbytes == 0) {
			/* play()==0 means failure */
			FormatError(error, "\"%s\" [%s] failed to play",
				    ao->name, ao->plugin->name);

			ao_close(ao, false);

			/* don't automatically reopen this device for
			   10 seconds */
			assert(ao->fail_timer == nullptr);
			ao->fail_timer = g_timer_new();

			return false;
		}

		assert(nbytes <= size);
		assert(nbytes % ao->out_audio_format.GetFrameSize() == 0);

		data += nbytes;
		size -= nbytes;
	}

	return true;
}

static const struct music_chunk *
ao_next_chunk(struct audio_output *ao)
{
	return ao->chunk != nullptr
		/* continue the previous play() call */
		? ao->chunk->next
		/* get the first chunk from the pipe */
		: ao->pipe->Peek();
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

	assert(ao->pipe != nullptr);

	chunk = ao_next_chunk(ao);
	if (chunk == nullptr)
		/* no chunk available */
		return false;

	ao->chunk_finished = false;

	while (chunk != nullptr && ao->command == AO_COMMAND_NONE) {
		assert(!ao->chunk_finished);

		ao->chunk = chunk;

		success = ao_play_chunk(ao, chunk);
		if (!success) {
			assert(ao->chunk == nullptr);
			break;
		}

		assert(ao->chunk == chunk);
		chunk = chunk->next;
	}

	ao->chunk_finished = true;

	ao->mutex.unlock();
	ao->player_control->LockSignal();
	ao->mutex.lock();

	return true;
}

static void ao_pause(struct audio_output *ao)
{
	bool ret;

	ao->mutex.unlock();
	ao_plugin_cancel(ao);
	ao->mutex.lock();

	ao->pause = true;
	ao_command_finished(ao);

	do {
		if (!ao_wait(ao))
			break;

		ao->mutex.unlock();
		ret = ao_plugin_pause(ao);
		ao->mutex.lock();

		if (!ret) {
			ao_close(ao, false);
			break;
		}
	} while (ao->command == AO_COMMAND_NONE);

	ao->pause = false;
}

static void
audio_output_task(void *arg)
{
	struct audio_output *ao = (struct audio_output *)arg;

	ao->mutex.lock();

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
			assert(ao->pipe != nullptr);

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
				assert(ao->chunk == nullptr);
				assert(ao->pipe->Peek() == nullptr);

				ao->mutex.unlock();
				ao_plugin_drain(ao);
				ao->mutex.lock();
			}

			ao_command_finished(ao);
			continue;

		case AO_COMMAND_CANCEL:
			ao->chunk = nullptr;

			if (ao->open) {
				ao->mutex.unlock();
				ao_plugin_cancel(ao);
				ao->mutex.lock();
			}

			ao_command_finished(ao);
			continue;

		case AO_COMMAND_KILL:
			ao->chunk = nullptr;
			ao_command_finished(ao);
			ao->mutex.unlock();
			return;
		}

		if (ao->open && ao->allow_play && ao_play(ao))
			/* don't wait for an event if there are more
			   chunks in the pipe */
			continue;

		if (ao->command == AO_COMMAND_NONE)
			ao->cond.wait(ao->mutex);
	}
}

void audio_output_thread_start(struct audio_output *ao)
{
	assert(ao->command == AO_COMMAND_NONE);

	Error error;
	if (!ao->thread.Start(audio_output_task, ao, error))
		FatalError(error);
}
