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
#include "OutputAPI.hxx"
#include "Domain.hxx"
#include "pcm/PcmMix.hxx"
#include "pcm/Domain.hxx"
#include "notify.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/plugins/ConvertFilterPlugin.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "PlayerControl.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "thread/Util.hxx"
#include "thread/Slack.hxx"
#include "thread/Name.hxx"
#include "system/FatalError.hxx"
#include "util/Error.hxx"
#include "util/ConstBuffer.hxx"
#include "Log.hxx"
#include "Compiler.h"

#include <assert.h>
#include <string.h>

void
AudioOutput::CommandFinished()
{
	assert(command != AO_COMMAND_NONE);
	command = AO_COMMAND_NONE;

	mutex.unlock();
	audio_output_client_notify.Signal();
	mutex.lock();
}

inline bool
AudioOutput::Enable()
{
	if (really_enabled)
		return true;

	mutex.unlock();
	Error error;
	bool success = ao_plugin_enable(this, error);
	mutex.lock();
	if (!success) {
		FormatError(error,
			    "Failed to enable \"%s\" [%s]",
			    name, plugin.name);
		return false;
	}

	really_enabled = true;
	return true;
}

inline void
AudioOutput::Disable()
{
	if (open)
		Close(false);

	if (really_enabled) {
		really_enabled = false;

		mutex.unlock();
		ao_plugin_disable(this);
		mutex.lock();
	}
}

inline AudioFormat
AudioOutput::OpenFilter(AudioFormat &format, Error &error_r)
{
	assert(format.IsValid());

	/* the replay_gain filter cannot fail here */
	if (replay_gain_filter != nullptr &&
	    !replay_gain_filter->Open(format, error_r).IsDefined())
		return AudioFormat::Undefined();

	if (other_replay_gain_filter != nullptr &&
	    !other_replay_gain_filter->Open(format, error_r).IsDefined()) {
		if (replay_gain_filter != nullptr)
			replay_gain_filter->Close();
		return AudioFormat::Undefined();
	}

	const AudioFormat af = filter->Open(format, error_r);
	if (!af.IsDefined()) {
		if (replay_gain_filter != nullptr)
			replay_gain_filter->Close();
		if (other_replay_gain_filter != nullptr)
			other_replay_gain_filter->Close();
	}

	return af;
}

void
AudioOutput::CloseFilter()
{
	if (replay_gain_filter != nullptr)
		replay_gain_filter->Close();
	if (other_replay_gain_filter != nullptr)
		other_replay_gain_filter->Close();

	filter->Close();
}

inline void
AudioOutput::Open()
{
	bool success;
	Error error;
	struct audio_format_string af_string;

	assert(!open);
	assert(pipe != nullptr);
	assert(current_chunk == nullptr);
	assert(in_audio_format.IsValid());

	fail_timer.Reset();

	/* enable the device (just in case the last enable has failed) */

	if (!Enable())
		/* still no luck */
		return;

	/* open the filter */

	const AudioFormat filter_audio_format =
		OpenFilter(in_audio_format, error);
	if (!filter_audio_format.IsDefined()) {
		FormatError(error, "Failed to open filter for \"%s\" [%s]",
			    name, plugin.name);

		fail_timer.Update();
		return;
	}

	assert(filter_audio_format.IsValid());

	out_audio_format = filter_audio_format;
	out_audio_format.ApplyMask(config_audio_format);

	mutex.unlock();

	const AudioFormat retry_audio_format = out_audio_format;

 retry_without_dsd:
	success = ao_plugin_open(this, out_audio_format, error);
	mutex.lock();

	assert(!open);

	if (!success) {
		FormatError(error, "Failed to open \"%s\" [%s]",
			    name, plugin.name);

		mutex.unlock();
		CloseFilter();
		mutex.lock();

		fail_timer.Update();
		return;
	}

	if (!convert_filter_set(convert_filter, out_audio_format,
				error)) {
		FormatError(error, "Failed to convert for \"%s\" [%s]",
			    name, plugin.name);

		mutex.unlock();
		ao_plugin_close(this);

		if (error.IsDomain(pcm_domain) &&
		    out_audio_format.format == SampleFormat::DSD) {
			/* if the audio output supports DSD, but not
			   the given sample rate, it asks MPD to
			   resample; resampling DSD however is not
			   implemented; our last resort is to give up
			   DSD and fall back to PCM */

			// TODO: clean up this workaround

			FormatError(output_domain, "Retrying without DSD");

			out_audio_format = retry_audio_format;
			out_audio_format.format = SampleFormat::FLOAT;

			/* clear the Error to allow reusing it */
			error.Clear();

			/* sorry for the "goto" - this is a workaround
			   for the stable branch that should be as
			   unintrusive as possible */
			goto retry_without_dsd;
		}

		CloseFilter();
		mutex.lock();

		fail_timer.Update();
		return;
	}

	open = true;

	FormatDebug(output_domain,
		    "opened plugin=%s name=\"%s\" audio_format=%s",
		    plugin.name, name,
		    audio_format_to_string(out_audio_format, &af_string));

	if (in_audio_format != out_audio_format)
		FormatDebug(output_domain, "converting from %s",
			    audio_format_to_string(in_audio_format,
						   &af_string));
}

void
AudioOutput::Close(bool drain)
{
	assert(open);

	pipe = nullptr;

	current_chunk = nullptr;
	open = false;

	mutex.unlock();

	if (drain)
		ao_plugin_drain(this);
	else
		ao_plugin_cancel(this);

	ao_plugin_close(this);
	CloseFilter();

	mutex.lock();

	FormatDebug(output_domain, "closed plugin=%s name=\"%s\"",
		    plugin.name, name);
}

void
AudioOutput::ReopenFilter()
{
	Error error;

	mutex.unlock();
	CloseFilter();
	mutex.lock();

	const AudioFormat filter_audio_format =
		OpenFilter(in_audio_format, error);
	if (!filter_audio_format.IsDefined() ||
	    !convert_filter_set(convert_filter, out_audio_format,
				error)) {
		FormatError(error,
			    "Failed to open filter for \"%s\" [%s]",
			    name, plugin.name);

		/* this is a little code duplication from Close(),
		   but we cannot call this function because we must
		   not call filter_close(filter) again */

		pipe = nullptr;

		current_chunk = nullptr;
		open = false;
		fail_timer.Update();

		mutex.unlock();
		ao_plugin_close(this);
		mutex.lock();

		return;
	}
}

void
AudioOutput::Reopen()
{
	if (!config_audio_format.IsFullyDefined()) {
		if (open) {
			const MusicPipe *mp = pipe;
			Close(true);
			pipe = mp;
		}

		/* no audio format is configured: copy in->out, let
		   the output's open() method determine the effective
		   out_audio_format */
		out_audio_format = in_audio_format;
		out_audio_format.ApplyMask(config_audio_format);
	}

	if (open)
		/* the audio format has changed, and all filters have
		   to be reconfigured */
		ReopenFilter();
	else
		Open();
}

/**
 * Wait until the output's delay reaches zero.
 *
 * @return true if playback should be continued, false if a command
 * was issued
 */
inline bool
AudioOutput::WaitForDelay()
{
	while (true) {
		unsigned delay = ao_plugin_delay(this);
		if (delay == 0)
			return true;

		(void)cond.timed_wait(mutex, delay);

		if (command != AO_COMMAND_NONE)
			return false;
	}
}

static ConstBuffer<void>
ao_chunk_data(AudioOutput *ao, const MusicChunk *chunk,
	      Filter *replay_gain_filter,
	      unsigned *replay_gain_serial_p)
{
	assert(chunk != nullptr);
	assert(!chunk->IsEmpty());
	assert(chunk->CheckFormat(ao->in_audio_format));

	ConstBuffer<void> data(chunk->data, chunk->length);

	(void)ao;

	assert(data.size % ao->in_audio_format.GetFrameSize() == 0);

	if (!data.IsEmpty() && replay_gain_filter != nullptr) {
		if (chunk->replay_gain_serial != *replay_gain_serial_p) {
			replay_gain_filter_set_info(replay_gain_filter,
						    chunk->replay_gain_serial != 0
						    ? &chunk->replay_gain_info
						    : nullptr);
			*replay_gain_serial_p = chunk->replay_gain_serial;
		}

		Error error;
		data = replay_gain_filter->FilterPCM(data, error);
		if (data.IsNull())
			FormatError(error, "\"%s\" [%s] failed to filter",
				    ao->name, ao->plugin.name);
	}

	return data;
}

static ConstBuffer<void>
ao_filter_chunk(AudioOutput *ao, const MusicChunk *chunk)
{
	ConstBuffer<void> data =
		ao_chunk_data(ao, chunk, ao->replay_gain_filter,
			      &ao->replay_gain_serial);
	if (data.IsEmpty())
		return data;

	/* cross-fade */

	if (chunk->other != nullptr) {
		ConstBuffer<void> other_data =
			ao_chunk_data(ao, chunk->other,
				      ao->other_replay_gain_filter,
				      &ao->other_replay_gain_serial);
		if (other_data.IsNull())
			return nullptr;

		if (other_data.IsEmpty())
			return data;

		/* if the "other" chunk is longer, then that trailer
		   is used as-is, without mixing; it is part of the
		   "next" song being faded in, and if there's a rest,
		   it means cross-fading ends here */

		if (data.size > other_data.size)
			data.size = other_data.size;

		float mix_ratio = chunk->mix_ratio;
		if (mix_ratio >= 0)
			/* reverse the mix ratio (because the
			   arguments to pcm_mix() are reversed), but
			   only if the mix ratio is non-negative; a
			   negative mix ratio is a MixRamp special
			   case */
			mix_ratio = 1.0 - mix_ratio;

		void *dest = ao->cross_fade_buffer.Get(other_data.size);
		memcpy(dest, other_data.data, other_data.size);
		if (!pcm_mix(ao->cross_fade_dither, dest, data.data, data.size,
			     ao->in_audio_format.format,
			     mix_ratio)) {
			FormatError(output_domain,
				    "Cannot cross-fade format %s",
				    sample_format_to_string(ao->in_audio_format.format));
			return nullptr;
		}

		data.data = dest;
		data.size = other_data.size;
	}

	/* apply filter chain */

	Error error;
	data = ao->filter->FilterPCM(data, error);
	if (data.IsNull()) {
		FormatError(error, "\"%s\" [%s] failed to filter",
			    ao->name, ao->plugin.name);
		return nullptr;
	}

	return data;
}

inline bool
AudioOutput::PlayChunk(const MusicChunk *chunk)
{
	assert(filter != nullptr);

	if (tags && gcc_unlikely(chunk->tag != nullptr)) {
		mutex.unlock();
		ao_plugin_send_tag(this, chunk->tag);
		mutex.lock();
	}

	auto data = ConstBuffer<char>::FromVoid(ao_filter_chunk(this, chunk));
	if (data.IsNull()) {
		Close(false);

		/* don't automatically reopen this device for 10
		   seconds */
		fail_timer.Update();
		return false;
	}

	Error error;

	while (!data.IsEmpty() && command == AO_COMMAND_NONE) {
		if (!WaitForDelay())
			break;

		mutex.unlock();
		size_t nbytes = ao_plugin_play(this, data.data, data.size,
					       error);
		mutex.lock();
		if (nbytes == 0) {
			/* play()==0 means failure */
			FormatError(error, "\"%s\" [%s] failed to play",
				    name, plugin.name);

			Close(false);

			/* don't automatically reopen this device for
			   10 seconds */
			assert(!fail_timer.IsDefined());
			fail_timer.Update();

			return false;
		}

		assert(nbytes <= data.size);
		assert(nbytes % out_audio_format.GetFrameSize() == 0);

		data.data += nbytes;
		data.size -= nbytes;
	}

	return true;
}

inline const MusicChunk *
AudioOutput::GetNextChunk() const
{
	return current_chunk != nullptr
		/* continue the previous play() call */
		? current_chunk->next
		/* get the first chunk from the pipe */
		: pipe->Peek();
}

inline bool
AudioOutput::Play()
{
	assert(pipe != nullptr);

	const MusicChunk *chunk = GetNextChunk();
	if (chunk == nullptr)
		/* no chunk available */
		return false;

	current_chunk_finished = false;

	assert(!in_playback_loop);
	in_playback_loop = true;

	while (chunk != nullptr && command == AO_COMMAND_NONE) {
		assert(!current_chunk_finished);

		current_chunk = chunk;

		if (!PlayChunk(chunk)) {
			assert(current_chunk == nullptr);
			break;
		}

		assert(current_chunk == chunk);
		chunk = chunk->next;
	}

	assert(in_playback_loop);
	in_playback_loop = false;

	current_chunk_finished = true;

	mutex.unlock();
	player_control->LockSignal();
	mutex.lock();

	return true;
}

inline void
AudioOutput::Pause()
{
	mutex.unlock();
	ao_plugin_cancel(this);
	mutex.lock();

	pause = true;
	CommandFinished();

	do {
		if (!WaitForDelay())
			break;

		mutex.unlock();
		bool success = ao_plugin_pause(this);
		mutex.lock();

		if (!success) {
			Close(false);
			break;
		}
	} while (command == AO_COMMAND_NONE);

	pause = false;
}

inline void
AudioOutput::Task()
{
	FormatThreadName("output:%s", name);

	SetThreadRealtime();
	SetThreadTimerSlackUS(100);

	mutex.lock();

	while (1) {
		switch (command) {
		case AO_COMMAND_NONE:
			break;

		case AO_COMMAND_ENABLE:
			Enable();
			CommandFinished();
			break;

		case AO_COMMAND_DISABLE:
			Disable();
			CommandFinished();
			break;

		case AO_COMMAND_OPEN:
			Open();
			CommandFinished();
			break;

		case AO_COMMAND_REOPEN:
			Reopen();
			CommandFinished();
			break;

		case AO_COMMAND_CLOSE:
			assert(open);
			assert(pipe != nullptr);

			Close(false);
			CommandFinished();
			break;

		case AO_COMMAND_PAUSE:
			if (!open) {
				/* the output has failed after
				   audio_output_all_pause() has
				   submitted the PAUSE command; bail
				   out */
				CommandFinished();
				break;
			}

			Pause();
			/* don't "break" here: this might cause
			   Play() to be called when command==CLOSE
			   ends the paused state - "continue" checks
			   the new command first */
			continue;

		case AO_COMMAND_DRAIN:
			if (open) {
				assert(current_chunk == nullptr);
				assert(pipe->Peek() == nullptr);

				mutex.unlock();
				ao_plugin_drain(this);
				mutex.lock();
			}

			CommandFinished();
			continue;

		case AO_COMMAND_CANCEL:
			current_chunk = nullptr;

			if (open) {
				mutex.unlock();
				ao_plugin_cancel(this);
				mutex.lock();
			}

			CommandFinished();
			continue;

		case AO_COMMAND_KILL:
			current_chunk = nullptr;
			CommandFinished();
			mutex.unlock();
			return;
		}

		if (open && allow_play && Play())
			/* don't wait for an event if there are more
			   chunks in the pipe */
			continue;

		if (command == AO_COMMAND_NONE) {
			woken_for_play = false;
			cond.wait(mutex);
		}
	}
}

void
AudioOutput::Task(void *arg)
{
	AudioOutput *ao = (AudioOutput *)arg;
	ao->Task();
}

void
AudioOutput::StartThread()
{
	assert(command == AO_COMMAND_NONE);

	Error error;
	if (!thread.Start(Task, this, error))
		FatalError(error);
}
