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
#include "Internal.hxx"
#include "OutputPlugin.hxx"
#include "Domain.hxx"
#include "Log.hxx"
#include "mixer/MixerInternal.hxx"
#include "mixer/plugins/SoftwareMixerPlugin.hxx"
#include "filter/plugins/ConvertFilterPlugin.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringBuffer.hxx"

void
AudioOutput::Enable()
{
	try {
		ao_plugin_enable(*this);
	} catch (const std::runtime_error &e) {
		std::throw_with_nested(FormatRuntimeError("Failed to enable output \"%s\" [%s]",
							  name, plugin.name));
	}
}

void
AudioOutput::Disable() noexcept
{
	ao_plugin_disable(*this);
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
AudioOutput::CloseOutput(bool drain) noexcept
{
	if (drain)
		ao_plugin_drain(*this);
	else
		ao_plugin_cancel(*this);

	ao_plugin_close(*this);
}

void
AudioOutput::CloseFilter() noexcept
{
	if (mixer != nullptr && mixer->IsPlugin(software_mixer_plugin))
		software_mixer_set_filter(*mixer, nullptr);
}

void
AudioOutput::Close(bool drain) noexcept
{
	const ScopeUnlock unlock(mutex);

	CloseOutput(drain);
	CloseFilter();

	FormatDebug(output_domain, "closed plugin=%s name=\"%s\"",
		    plugin.name, name);
}

void
AudioOutput::BeginPause() noexcept
{
	const ScopeUnlock unlock(mutex);
	ao_plugin_cancel(*this);
}

bool
AudioOutput::IteratePause() noexcept
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

	return success;
}
