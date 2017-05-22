/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Control.hxx"
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
#include "util/StringBuffer.hxx"
#include "util/ScopeExit.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"
#include "Compiler.h"

#include <stdexcept>

#include <assert.h>
#include <string.h>

void
AudioOutputControl::CommandFinished()
{
	assert(command != Command::NONE);
	command = Command::NONE;

	const ScopeUnlock unlock(mutex);
	audio_output_client_notify.Signal();
}

inline void
AudioOutput::Enable()
{
	if (really_enabled)
		return;

	try {
		const ScopeUnlock unlock(mutex);
		ao_plugin_enable(*this);
	} catch (const std::runtime_error &e) {
		std::throw_with_nested(FormatRuntimeError("Failed to enable output \"%s\" [%s]",
							  name, plugin.name));
	}

	really_enabled = true;
}

inline void
AudioOutput::Disable()
{
	if (open)
		Close(false);

	if (really_enabled) {
		really_enabled = false;

		const ScopeUnlock unlock(mutex);
		ao_plugin_disable(*this);
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
AudioOutput::Open(AudioFormat audio_format, const MusicPipe &pipe)
{
	assert(audio_format.IsValid());

	/* enable the device (just in case the last enable has failed) */
	Enable();

	AudioFormat f;

	try {
		f = source.Open(audio_format, pipe,
				prepared_replay_gain_filter,
				prepared_other_replay_gain_filter,
				prepared_filter);

		if (mixer != nullptr && mixer->IsPlugin(software_mixer_plugin))
			software_mixer_set_filter(*mixer, volume_filter.Get());
	} catch (const std::runtime_error &e) {
		std::throw_with_nested(FormatRuntimeError("Failed to open filter for \"%s\" [%s]",
							  name, plugin.name));
	}

	const auto cf = f.WithMask(config_audio_format);

	if (open && cf != filter_audio_format) {
		/* if the filter's output format changes, the output
		   must be reopened as well */
		CloseOutput(true);
		open = false;
	}

	filter_audio_format = cf;

	if (!open) {
		try {
			OpenOutputAndConvert(filter_audio_format);
		} catch (...) {
			CloseFilter();
			throw;
		}

		open = true;
	} else if (f != out_audio_format) {
		/* reconfigure the final ConvertFilter for its new
		   input AudioFormat */

		try {
			convert_filter_set(convert_filter.Get(),
					   out_audio_format);
		} catch (const std::runtime_error &e) {
			Close(false);
			std::throw_with_nested(FormatRuntimeError("Failed to convert for \"%s\" [%s]",
								  name, plugin.name));
		}
	}

	if (f != source.GetInputAudioFormat() || f != out_audio_format)
		FormatDebug(output_domain, "converting in=%s -> f=%s -> out=%s",
			    ToString(source.GetInputAudioFormat()).c_str(),
			    ToString(f).c_str(),
			    ToString(out_audio_format).c_str());
}

void
AudioOutput::OpenOutputAndConvert(AudioFormat desired_audio_format)
{
	out_audio_format = desired_audio_format;

	try {
		ao_plugin_open(*this, out_audio_format);
	} catch (const std::runtime_error &e) {
		std::throw_with_nested(FormatRuntimeError("Failed to open \"%s\" [%s]",
							  name, plugin.name));
	}

	FormatDebug(output_domain,
		    "opened plugin=%s name=\"%s\" audio_format=%s",
		    plugin.name, name,
		    ToString(out_audio_format).c_str());

	try {
		convert_filter_set(convert_filter.Get(), out_audio_format);
	} catch (const std::runtime_error &e) {
		ao_plugin_close(*this);

		if (out_audio_format.format == SampleFormat::DSD) {
			/* if the audio output supports DSD, but not
			   the given sample rate, it asks MPD to
			   resample; resampling DSD however is not
			   implemented; our last resort is to give up
			   DSD and fall back to PCM */

			LogError(e);
			FormatError(output_domain, "Retrying without DSD");

			desired_audio_format.format = SampleFormat::FLOAT;
			OpenOutputAndConvert(desired_audio_format);
			return;
		}

		std::throw_with_nested(FormatRuntimeError("Failed to convert for \"%s\" [%s]",
							  name, plugin.name));
	}
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
		ao_plugin_drain(*this);
	else
		ao_plugin_cancel(*this);

	ao_plugin_close(*this);
}

/**
 * Wait until the output's delay reaches zero.
 *
 * @return true if playback should be continued, false if a command
 * was issued
 */
inline bool
AudioOutputControl::WaitForDelay()
{
	while (true) {
		const auto delay = ao_plugin_delay(*output);
		if (delay <= std::chrono::steady_clock::duration::zero())
			return true;

		(void)cond.timed_wait(mutex, delay);

		if (command != Command::NONE)
			return false;
	}
}

bool
AudioOutputControl::FillSourceOrClose()
try {
	return output->source.Fill(mutex);
} catch (const std::runtime_error &e) {
	FormatError(e, "Failed to filter for output \"%s\" [%s]",
		    GetName(), output->plugin.name);

	output->Close(false);

	/* don't automatically reopen this device for 10
	   seconds */
	fail_timer.Update();
	return false;
}

inline bool
AudioOutputControl::PlayChunk()
{
	if (tags) {
		const auto *tag = output->source.ReadTag();
		if (tag != nullptr) {
			const ScopeUnlock unlock(mutex);
			try {
				ao_plugin_send_tag(*output, *tag);
			} catch (const std::runtime_error &e) {
				FormatError(e, "Failed to send tag to \"%s\" [%s]",
					    GetName(), output->plugin.name);
			}
		}
	}

	while (command == Command::NONE) {
		const auto data = output->source.PeekData();
		if (data.IsEmpty())
			break;

		if (!WaitForDelay())
			break;

		size_t nbytes;

		try {
			const ScopeUnlock unlock(mutex);
			nbytes = ao_plugin_play(*output, data.data, data.size);
			assert(nbytes <= data.size);
		} catch (const std::runtime_error &e) {
			FormatError(e, "\"%s\" [%s] failed to play",
				    GetName(), output->plugin.name);

			nbytes = 0;
		}

		if (nbytes == 0) {
			output->Close(false);

			/* don't automatically reopen this device for
			   10 seconds */
			assert(!fail_timer.IsDefined());
			fail_timer.Update();

			return false;
		}

		assert(nbytes % output->out_audio_format.GetFrameSize() == 0);

		output->source.ConsumeData(nbytes);
	}

	return true;
}

inline bool
AudioOutputControl::Play()
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
			output->client->ChunksConsumed();
			n = 0;
		}

		if (!PlayChunk())
			break;
	} while (FillSourceOrClose());

	const ScopeUnlock unlock(mutex);
	output->client->ChunksConsumed();

	return true;
}

inline void
AudioOutput::BeginPause()
{
	{
		const ScopeUnlock unlock(mutex);
		ao_plugin_cancel(*this);
	}

	pause = true;
}

inline bool
AudioOutput::IteratePause()
{
	bool success;

	try {
		const ScopeUnlock unlock(mutex);
		success = ao_plugin_pause(*this);
	} catch (const std::runtime_error &e) {
		FormatError(e, "\"%s\" [%s] failed to pause",
			    name, plugin.name);
		success = false;
	}

	if (!success)
		Close(false);

	return success;
}

inline void
AudioOutputControl::Pause()
{
	output->BeginPause();
	CommandFinished();

	do {
		if (!WaitForDelay())
			break;

		if (!output->IteratePause())
			break;
	} while (command == Command::NONE);

	output->EndPause();
}

void
AudioOutputControl::Task()
{
	FormatThreadName("output:%s", GetName());

	try {
		SetThreadRealtime();
	} catch (const std::runtime_error &e) {
		LogError(e,
			 "OutputThread could not get realtime scheduling, continuing anyway");
	}

	SetThreadTimerSlackUS(100);

	const std::lock_guard<Mutex> lock(mutex);

	while (true) {
		switch (command) {
		case Command::NONE:
			break;

		case Command::ENABLE:
			last_error = nullptr;

			try {
				output->Enable();
			} catch (const std::runtime_error &e) {
				LogError(e);
				fail_timer.Update();
				last_error = std::current_exception();
			}

			CommandFinished();
			break;

		case Command::DISABLE:
			output->Disable();
			CommandFinished();
			break;

		case Command::OPEN:
			last_error = nullptr;
			fail_timer.Reset();

			try {
				output->Open(request.audio_format,
					     *request.pipe);
			} catch (const std::runtime_error &e) {
				LogError(e);
				fail_timer.Update();
				last_error = std::current_exception();
			}

			CommandFinished();
			break;

		case Command::CLOSE:
			if (IsOpen())
				output->Close(false);
			CommandFinished();
			break;

		case Command::PAUSE:
			if (!output->open) {
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
			if (output->open) {
				const ScopeUnlock unlock(mutex);
				ao_plugin_drain(*output);
			}

			CommandFinished();
			continue;

		case Command::CANCEL:
			output->source.Cancel();

			if (output->open) {
				const ScopeUnlock unlock(mutex);
				ao_plugin_cancel(*output);
			}

			CommandFinished();
			continue;

		case Command::KILL:
			output->Disable();
			output->source.Cancel();
			CommandFinished();
			return;
		}

		if (output->open && allow_play && Play())
			/* don't wait for an event if there are more
			   chunks in the pipe */
			continue;

		if (command == Command::NONE) {
			woken_for_play = false;
			cond.wait(output->mutex);
		}
	}
}

void
AudioOutputControl::StartThread()
{
	assert(command == Command::NONE);

	thread.Start();
}
