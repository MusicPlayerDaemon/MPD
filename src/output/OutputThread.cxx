/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "notify.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/plugins/ConvertFilterPlugin.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "mixer/MixerInternal.hxx"
#include "mixer/plugins/SoftwareMixerPlugin.hxx"
#include "player/Control.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "thread/Util.hxx"
#include "thread/Slack.hxx"
#include "thread/Name.hxx"
#include "util/ConstBuffer.hxx"
#include "util/ScopeExit.hxx"
#include "Log.hxx"
#include "Compiler.h"

#include <stdexcept>

#include <assert.h>
#include <string.h>

void
AudioOutput::CommandFinished()
{
	assert(command != Command::NONE);
	command = Command::NONE;

	const ScopeUnlock unlock(mutex);
	audio_output_client_notify.Signal();
}

inline bool
AudioOutput::Enable()
{
	if (really_enabled)
		return true;

	try {
		const ScopeUnlock unlock(mutex);
		ao_plugin_enable(this);
	} catch (const std::runtime_error &e) {
		FormatError(e,
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

		const ScopeUnlock unlock(mutex);
		ao_plugin_disable(this);
	}
}

inline AudioFormat
AudioOutput::OpenFilter(AudioFormat &format)
try {
	assert(format.IsValid());

	/* the replay_gain filter cannot fail here */
	if (prepared_replay_gain_filter != nullptr)
		replay_gain_filter_instance =
			prepared_replay_gain_filter->Open(format);

	if (prepared_other_replay_gain_filter != nullptr)
		other_replay_gain_filter_instance =
			prepared_other_replay_gain_filter->Open(format);

	filter_instance = prepared_filter->Open(format);

	if (mixer != nullptr && mixer->IsPlugin(software_mixer_plugin))
		software_mixer_set_filter(*mixer, volume_filter.Get());

	return filter_instance->GetOutAudioFormat();
} catch (...) {
	CloseFilter();
	throw;
}

void
AudioOutput::CloseFilter()
{
	if (mixer != nullptr && mixer->IsPlugin(software_mixer_plugin))
		software_mixer_set_filter(*mixer, nullptr);

	delete replay_gain_filter_instance;
	replay_gain_filter_instance = nullptr;

	delete other_replay_gain_filter_instance;
	other_replay_gain_filter_instance = nullptr;

	delete filter_instance;
	filter_instance = nullptr;
}

inline void
AudioOutput::Open()
{
	assert(!open);
	assert(in_audio_format.IsValid());

	fail_timer.Reset();

	/* enable the device (just in case the last enable has failed) */

	if (!Enable())
		/* still no luck */
		return;

	bool success;

	{
		const ScopeUnlock unlock(mutex);
		success = OpenFilterAndOutput();
	}

	if (success)
		open = true;
	else
		fail_timer.Update();
}

bool
AudioOutput::OpenFilterAndOutput()
{
	AudioFormat filter_audio_format;
	try {
		filter_audio_format = OpenFilter(in_audio_format);
	} catch (const std::runtime_error &e) {
		FormatError(e, "Failed to open filter for \"%s\" [%s]",
			    name, plugin.name);
		return false;
	}

	assert(filter_audio_format.IsValid());

	const auto audio_format =
		filter_audio_format.WithMask(config_audio_format);
	bool success = OpenOutputAndConvert(audio_format);
	if (!success)
		CloseFilter();

	return success;
}

bool
AudioOutput::OpenOutputAndConvert(AudioFormat desired_audio_format)
{
	out_audio_format = desired_audio_format;

	try {
		ao_plugin_open(this, out_audio_format);
	} catch (const std::runtime_error &e) {
		FormatError(e, "Failed to open \"%s\" [%s]",
			    name, plugin.name);
		return false;
	}

	try {
		convert_filter_set(convert_filter.Get(), out_audio_format);
	} catch (const std::runtime_error &e) {
		FormatError(e, "Failed to convert for \"%s\" [%s]",
			    name, plugin.name);

		ao_plugin_close(this);

		if (out_audio_format.format == SampleFormat::DSD) {
			/* if the audio output supports DSD, but not
			   the given sample rate, it asks MPD to
			   resample; resampling DSD however is not
			   implemented; our last resort is to give up
			   DSD and fall back to PCM */

			FormatError(output_domain, "Retrying without DSD");

			desired_audio_format.format = SampleFormat::FLOAT;
			return OpenOutputAndConvert(desired_audio_format);
		}

		return false;
	}

	struct audio_format_string af_string;
	FormatDebug(output_domain,
		    "opened plugin=%s name=\"%s\" audio_format=%s",
		    plugin.name, name,
		    audio_format_to_string(out_audio_format, &af_string));

	if (in_audio_format != out_audio_format)
		FormatDebug(output_domain, "converting from %s",
			    audio_format_to_string(in_audio_format,
						   &af_string));

	return true;
}

void
AudioOutput::Close(bool drain)
{
	assert(open);

	pipe.Cancel();

	open = false;

	const ScopeUnlock unlock(mutex);

	CloseOutput(drain);
	CloseFilter();

	FormatDebug(output_domain, "closed plugin=%s name=\"%s\"",
		    plugin.name, name);
}

inline void
AudioOutput::CloseOutput(bool drain)
{
	if (drain)
		ao_plugin_drain(this);
	else
		ao_plugin_cancel(this);

	ao_plugin_close(this);
}

void
AudioOutput::ReopenFilter()
{
	try {
		const ScopeUnlock unlock(mutex);
		CloseFilter();
		OpenFilter(in_audio_format);
		convert_filter_set(convert_filter.Get(), out_audio_format);
	} catch (const std::runtime_error &e) {
		FormatError(e,
			    "Failed to open filter for \"%s\" [%s]",
			    name, plugin.name);

		Close(false);
	}
}

void
AudioOutput::Reopen()
{
	assert(open);

	if (!config_audio_format.IsFullyDefined()) {
		Close(true);
		Open();
	} else
		/* the audio format has changed, and all filters have
		   to be reconfigured */
		ReopenFilter();
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

		if (command != Command::NONE)
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
		replay_gain_filter_set_mode(*replay_gain_filter,
					    ao->replay_gain_mode);

		if (chunk->replay_gain_serial != *replay_gain_serial_p) {
			replay_gain_filter_set_info(*replay_gain_filter,
						    chunk->replay_gain_serial != 0
						    ? &chunk->replay_gain_info
						    : nullptr);
			*replay_gain_serial_p = chunk->replay_gain_serial;
		}

		try {
			data = replay_gain_filter->FilterPCM(data);
		} catch (const std::runtime_error &e) {
			FormatError(e, "\"%s\" [%s] failed to filter",
				    ao->name, ao->plugin.name);
			return nullptr;
		}
	}

	return data;
}

static ConstBuffer<void>
ao_filter_chunk(AudioOutput *ao, const MusicChunk *chunk)
{
	ConstBuffer<void> data =
		ao_chunk_data(ao, chunk, ao->replay_gain_filter_instance,
			      &ao->replay_gain_serial);
	if (data.IsEmpty())
		return data;

	/* cross-fade */

	if (chunk->other != nullptr) {
		ConstBuffer<void> other_data =
			ao_chunk_data(ao, chunk->other,
				      ao->other_replay_gain_filter_instance,
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

	try {
		return ao->filter_instance->FilterPCM(data);
	} catch (const std::runtime_error &e) {
		FormatError(e, "\"%s\" [%s] failed to filter",
			    ao->name, ao->plugin.name);
		return nullptr;
	}
}

inline bool
AudioOutput::PlayChunk(const MusicChunk *chunk)
{
	assert(filter_instance != nullptr);

	if (tags && gcc_unlikely(chunk->tag != nullptr)) {
		const ScopeUnlock unlock(mutex);
		try {
			ao_plugin_send_tag(this, *chunk->tag);
		} catch (const std::runtime_error &e) {
			FormatError(e, "Failed to send tag to \"%s\" [%s]",
				    name, plugin.name);
		}
	}

	auto data = ConstBuffer<char>::FromVoid(ao_filter_chunk(this, chunk));
	if (data.IsNull()) {
		Close(false);

		/* don't automatically reopen this device for 10
		   seconds */
		fail_timer.Update();
		return false;
	}

	while (!data.IsEmpty() && command == Command::NONE) {
		if (!WaitForDelay())
			break;

		size_t nbytes;

		try {
			const ScopeUnlock unlock(mutex);
			nbytes = ao_plugin_play(this, data.data, data.size);
		} catch (const std::runtime_error &e) {
			FormatError(e, "\"%s\" [%s] failed to play",
				    name, plugin.name);

			nbytes = 0;
		}

		if (nbytes == 0) {
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

inline bool
AudioOutput::Play()
{
	const MusicChunk *chunk = pipe.Get();
	if (chunk == nullptr)
		/* no chunk available */
		return false;

	assert(!in_playback_loop);
	in_playback_loop = true;

	AtScopeExit(this) {
		assert(in_playback_loop);
		in_playback_loop = false;
	};

	unsigned n = 0;

	do {
		if (command != Command::NONE)
			return true;

		if (++n >= 64) {
			/* wake up the player every now and then to
			   give it a chance to refill the pipe before
			   it runs empty */
			const ScopeUnlock unlock(mutex);
			player_control->LockSignal();
			n = 0;
		}

		if (!PlayChunk(chunk))
			break;

		pipe.Consume(*chunk);
		chunk = pipe.Get();
	} while (chunk != nullptr);

	const ScopeUnlock unlock(mutex);
	player_control->LockSignal();

	return true;
}

inline void
AudioOutput::Pause()
{
	{
		const ScopeUnlock unlock(mutex);
		ao_plugin_cancel(this);
	}

	pause = true;
	CommandFinished();

	do {
		if (!WaitForDelay())
			break;

		bool success;
		try {
			const ScopeUnlock unlock(mutex);
			success = ao_plugin_pause(this);
		} catch (const std::runtime_error &e) {
			FormatError(e, "\"%s\" [%s] failed to pause",
				    name, plugin.name);
			success = false;
		}

		if (!success) {
			Close(false);
			break;
		}
	} while (command == Command::NONE);

	pause = false;
}

inline void
AudioOutput::Task()
{
	FormatThreadName("output:%s", name);

	try {
		SetThreadRealtime();
	} catch (const std::runtime_error &e) {
		LogError(e,
			 "OutputThread could not get realtime scheduling, continuing anyway");
	}

	SetThreadTimerSlackUS(100);

	const ScopeLock lock(mutex);

	while (1) {
		switch (command) {
		case Command::NONE:
			break;

		case Command::ENABLE:
			Enable();
			CommandFinished();
			break;

		case Command::DISABLE:
			Disable();
			CommandFinished();
			break;

		case Command::OPEN:
			if (open)
				Reopen();
			else
				Open();
			CommandFinished();
			break;

		case Command::CLOSE:
			assert(open);

			Close(false);
			CommandFinished();
			break;

		case Command::PAUSE:
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

		case Command::DRAIN:
			if (open) {
				assert(pipe.IsInitial());
				assert(pipe.GetPipe().Peek() == nullptr);

				const ScopeUnlock unlock(mutex);
				ao_plugin_drain(this);
			}

			CommandFinished();
			continue;

		case Command::CANCEL:
			pipe.Cancel();

			if (open) {
				const ScopeUnlock unlock(mutex);
				ao_plugin_cancel(this);
			}

			CommandFinished();
			continue;

		case Command::KILL:
			pipe.Cancel();
			CommandFinished();
			return;
		}

		if (open && allow_play && Play())
			/* don't wait for an event if there are more
			   chunks in the pipe */
			continue;

		if (command == Command::NONE) {
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
	assert(command == Command::NONE);

	thread.Start(Task, this);
}
