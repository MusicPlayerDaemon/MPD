// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Filtered.hxx"
#include "Interface.hxx"
#include "Domain.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "mixer/Control.hxx"
#include "mixer/Mixer.hxx"
#include "mixer/plugins/SoftwareMixerPlugin.hxx"
#include "filter/Prepared.hxx"
#include "filter/plugins/ConvertFilterPlugin.hxx"
#include "util/StringBuffer.hxx"
#include "Log.hxx"

FilteredAudioOutput::~FilteredAudioOutput()
{
	if (mixer != nullptr)
		mixer_free(mixer);
}

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

std::map<std::string, std::string, std::less<>>
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
		std::throw_with_nested(FmtRuntimeError("Failed to enable output {}",
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
		std::throw_with_nested(FmtRuntimeError("Failed to convert for {}",
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
		std::throw_with_nested(FmtRuntimeError("Failed to open {}",
						       GetLogName()));
	}

	FmtDebug(output_domain,
		 "opened {} audio_format={}",
		 GetLogName(), out_audio_format);

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
			LogError(output_domain, "Retrying without DSD");

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
			FmtError(output_domain,
				 "Failed to drain {}: {}",
				 GetLogName(), std::current_exception());
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

	FmtDebug(output_domain, "closed {}", GetLogName());
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

std::size_t
FilteredAudioOutput::Play(std::span<const std::byte> src)
{
	return output->Play(src);
}

void
FilteredAudioOutput::Drain()
{
	output->Drain();
}

void
FilteredAudioOutput::Interrupt() noexcept
{
	output->Interrupt();
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
FilteredAudioOutput::IteratePause()
{
	return output->Pause();
}
