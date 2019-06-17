/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Filtered.hxx"
#include "Interface.hxx"
#include "Domain.hxx"
#include "Log.hxx"
#include "mixer/MixerInternal.hxx"
#include "mixer/plugins/SoftwareMixerPlugin.hxx"
#include "filter/plugins/ConvertFilterPlugin.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringBuffer.hxx"

bool
FilteredAudioOutput::SupportsEnableDisable() const noexcept
{
	return output->SupportsEnableDisable();
}

bool
FilteredAudioOutput::SupportsPause() const noexcept
{
	return output->SupportsPause();
}

const std::map<std::string, std::string>
FilteredAudioOutput::GetAttributes() const noexcept
{
	return output->GetAttributes();
}

void
FilteredAudioOutput::SetAttribute(std::string &&_name, std::string &&_value)
{
	output->SetAttribute(std::move(_name), std::move(_value));
}

void
FilteredAudioOutput::Enable()
{
	try {
		output->Enable();
	} catch (...) {
		std::throw_with_nested(FormatRuntimeError("Failed to enable output %s",
							  GetLogName()));
	}
}

void
FilteredAudioOutput::Disable() noexcept
{
	output->Disable();
}

void
FilteredAudioOutput::ConfigureConvertFilter()
{
	try {
		convert_filter_set(convert_filter.Get(), out_audio_format);
	} catch (...) {
		std::throw_with_nested(FormatRuntimeError("Failed to convert for %s",
							  GetLogName()));
	}
}

void
FilteredAudioOutput::OpenOutputAndConvert(AudioFormat desired_audio_format)
{
	out_audio_format = desired_audio_format;

	try {
		output->Open(out_audio_format);
	} catch (...) {
		std::throw_with_nested(FormatRuntimeError("Failed to open %s",
							  GetLogName()));
	}

	FormatDebug(output_domain,
		    "opened %s audio_format=%s",
		    GetLogName(),
		    ToString(out_audio_format).c_str());

	try {
		ConfigureConvertFilter();
	} catch (...) {
		output->Close();

		if (out_audio_format.format == SampleFormat::DSD) {
			/* if the audio output supports DSD, but not
			   the given sample rate, it asks MPD to
			   resample; resampling DSD however is not
			   implemented; our last resort is to give up
			   DSD and fall back to PCM */

			LogError(std::current_exception());
			FormatError(output_domain, "Retrying without DSD");

			desired_audio_format.format = SampleFormat::FLOAT;
			OpenOutputAndConvert(desired_audio_format);
			return;
		}

		throw;
	}
}

void
FilteredAudioOutput::CloseOutput(bool drain) noexcept
{
	if (drain) {
		try {
			Drain();
		} catch (...) {
			FormatError(std::current_exception(),
				    "Failed to drain %s", GetLogName());
		}
	} else
		Cancel();

	output->Close();
}

void
FilteredAudioOutput::OpenSoftwareMixer() noexcept
{
	if (mixer != nullptr && mixer->IsPlugin(software_mixer_plugin))
		software_mixer_set_filter(*mixer, volume_filter.Get());
}

void
FilteredAudioOutput::CloseSoftwareMixer() noexcept
{
	if (mixer != nullptr && mixer->IsPlugin(software_mixer_plugin))
		software_mixer_set_filter(*mixer, nullptr);
}

void
FilteredAudioOutput::Close(bool drain) noexcept
{
	CloseOutput(drain);
	CloseSoftwareMixer();

	FormatDebug(output_domain, "closed %s", GetLogName());
}

std::chrono::steady_clock::duration
FilteredAudioOutput::Delay() noexcept
{
	return output->Delay();
}

void
FilteredAudioOutput::SendTag(const Tag &tag)
{
	output->SendTag(tag);
}

size_t
FilteredAudioOutput::Play(const void *data, size_t size)
{
	return output->Play(data, size);
}

void
FilteredAudioOutput::Drain()
{
	output->Drain();
}

void
FilteredAudioOutput::Cancel() noexcept
{
	output->Cancel();
}

void
FilteredAudioOutput::BeginPause() noexcept
{
	Cancel();
}

bool
FilteredAudioOutput::IteratePause() noexcept
{
	try {
		return output->Pause();
	} catch (...) {
		FormatError(std::current_exception(), "Failed to pause %s",
			    GetLogName());
		return false;
	}
}
