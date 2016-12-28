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
#include "Client.hxx"
#include "OutputAPI.hxx"
#include "Domain.hxx"
#include "pcm/PcmMix.hxx"
#include "notify.hxx"
#include "filter/FilterInternal.hxx"
#include "filter/plugins/ConvertFilterPlugin.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "mixer/MixerInternal.hxx"
#include "mixer/plugins/SoftwareMixerPlugin.hxx"
#include "MusicPipe.hxx"
#include "MusicChunk.hxx"
#include "thread/Util.hxx"
#include "thread/Slack.hxx"
#include "thread/Name.hxx"
#include "util/ConstBuffer.hxx"
#include "util/ScopeExit.hxx"
#include "util/RuntimeError.hxx"
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

void
AudioOutput::CloseFilter()
{
	if (mixer != nullptr && mixer->IsPlugin(software_mixer_plugin))
		software_mixer_set_filter(*mixer, nullptr);

	source.Close();
}

inline void
AudioOutput::Open()
{
	assert(request.audio_format.IsValid());

	fail_timer.Reset();

	/* enable the device (just in case the last enable has failed) */
	if (!Enable()) {
		/* still no luck */
		fail_timer.Update();
		return;
	}

	AudioFormat f;

	try {
		f = source.Open(request.audio_format, *request.pipe,
				prepared_replay_gain_filter,
				prepared_other_replay_gain_filter,
				prepared_filter)
			.WithMask(config_audio_format);

		if (mixer != nullptr && mixer->IsPlugin(software_mixer_plugin))
			software_mixer_set_filter(*mixer, volume_filter.Get());
	} catch (const std::runtime_error &e) {
		FormatError(e, "Failed to open filter for \"%s\" [%s]",
			    name, plugin.name);
		fail_timer.Update();
		return;
	}

	if (open && f != filter_audio_format) {
		/* if the filter's output format changes, the output
		   must be reopened as well */
		CloseOutput(true);
		open = false;
	}

	filter_audio_format = f;

	if (!open) {
		if (OpenOutputAndConvert(filter_audio_format)) {
			open = true;
		} else {
			CloseFilter();
			fail_timer.Update();
		}
	}
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

	if (source.GetInputAudioFormat() != out_audio_format)
		FormatDebug(output_domain, "converting from %s",
			    audio_format_to_string(source.GetInputAudioFormat(),
						   &af_string));

	return true;
}

void
AudioOutput::Close(bool drain)
{
	assert(open);

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
		const auto delay = ao_plugin_delay(this);
		if (delay <= std::chrono::steady_clock::duration::zero())
			return true;

		(void)cond.timed_wait(mutex, delay);

		if (command != Command::NONE)
			return false;
	}
}

bool
AudioOutput::FillSourceOrClose()
try {
	return source.Fill(mutex);
} catch (const std::runtime_error &e) {
	FormatError(e, "Failed to filter for output \"%s\" [%s]",
		    name, plugin.name);

	Close(false);

	/* don't automatically reopen this device for 10
	   seconds */
	fail_timer.Update();
	return false;
}

inline bool
AudioOutput::PlayChunk()
{
	if (tags) {
		const auto *tag = source.ReadTag();
		if (tag != nullptr) {
			const ScopeUnlock unlock(mutex);
			try {
				ao_plugin_send_tag(this, *tag);
			} catch (const std::runtime_error &e) {
				FormatError(e, "Failed to send tag to \"%s\" [%s]",
					    name, plugin.name);
			}
		}
	}

	while (command == Command::NONE) {
		const auto data = source.PeekData();
		if (data.IsEmpty())
			break;

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

		assert(nbytes % out_audio_format.GetFrameSize() == 0);

		source.ConsumeData(nbytes);
	}

	return true;
}

inline bool
AudioOutput::Play()
{
	if (!FillSourceOrClose())
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
			client->ChunksConsumed();
			n = 0;
		}

		if (!PlayChunk())
			break;
	} while (FillSourceOrClose());

	const ScopeUnlock unlock(mutex);
	client->ChunksConsumed();

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

	while (true) {
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
			Open();
			CommandFinished();
			break;

		case Command::CLOSE:
			if (open)
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
				const ScopeUnlock unlock(mutex);
				ao_plugin_drain(this);
			}

			CommandFinished();
			continue;

		case Command::CANCEL:
			source.Cancel();

			if (open) {
				const ScopeUnlock unlock(mutex);
				ao_plugin_cancel(this);
			}

			CommandFinished();
			continue;

		case Command::KILL:
			Disable();
			source.Cancel();
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
