// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "output/MultipleOutputs.hxx"
#include "Control.hxx"
#include "Mixer.hxx"
#include "plugins/NullMixerPlugin.hxx"
#include "plugins/SoftwareMixerPlugin.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "pcm/Volume.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <cassert>

static constexpr Domain mixer_domain("mixer");

[[gnu::pure]]
static int
output_mixer_get_volume(const AudioOutputControl &ao) noexcept
{
	auto *mixer = ao.GetMixer();
	if (mixer == nullptr)
		return -1;

	/* software mixers are always considered, even if they are
	   disabled */
	if (!ao.IsEnabled() && !mixer->IsPlugin(software_mixer_plugin))
		return -1;

	try {
		return mixer->LockGetVolume();
	} catch (...) {
		FmtError(mixer_domain,
			 "Failed to read mixer for {:?}: {}",
			 ao.GetName(), std::current_exception());
		return -1;
	}
}

int
MultipleOutputs::GetVolume() const noexcept
{
	unsigned ok = 0;
	int total = 0;

	for (const auto &ao : outputs) {
		int volume = output_mixer_get_volume(*ao);
		if (volume >= 0) {
			total += volume;
			++ok;
		}
	}

	if (ok == 0)
		return -1;

	return total / ok;
}

enum class SetVolumeResult {
	NO_MIXER,
	DISABLED,
	ERROR,
	OK,
};

static SetVolumeResult
output_mixer_set_volume(AudioOutputControl &ao, unsigned volume)
{
	assert(volume <= 100);

	auto *mixer = ao.GetMixer();
	if (mixer == nullptr)
		return SetVolumeResult::NO_MIXER;

	/* software mixers are always updated, even if they are
	   disabled */
	if (!mixer->IsPlugin(software_mixer_plugin) &&
	    /* "global" mixers can be used even if the output hasn't
	       been used yet */
	    !(mixer->IsGlobal() ? ao.IsEnabled() : ao.IsReallyEnabled()))
		return SetVolumeResult::DISABLED;

	try {
		mixer->LockSetVolume(volume);
		return SetVolumeResult::OK;
	} catch (...) {
		FmtError(mixer_domain,
			 "Failed to set mixer for {:?}: {}",
			 ao.GetName(), std::current_exception());
		std::throw_with_nested(std::runtime_error(FmtBuffer<256>("Failed to set mixer for {:?}",
									 ao.GetName())));
	}
}

void
MultipleOutputs::SetVolume(unsigned volume)
{
	assert(volume <= 100);

	SetVolumeResult result = SetVolumeResult::NO_MIXER;
	std::exception_ptr error;

	for (const auto &ao : outputs) {
		try {
			auto r = output_mixer_set_volume(*ao, volume);
			if (r > result)
				result = r;
		} catch (...) {
			/* remember the first error */
			if (!error) {
				error = std::current_exception();
				result = SetVolumeResult::ERROR;
			}
		}
	}

	switch (result) {
	case SetVolumeResult::NO_MIXER:
		throw std::runtime_error{"No mixer"};

	case SetVolumeResult::DISABLED:
		throw std::runtime_error{"All outputs are disabled"};

	case SetVolumeResult::ERROR:
		std::rethrow_exception(error);

	case SetVolumeResult::OK:
		break;
	}
}

static int
output_mixer_get_software_volume(const AudioOutputControl &ao) noexcept
{
	if (!ao.IsEnabled())
		return -1;

	auto *mixer = ao.GetMixer();
	if (mixer == nullptr || !mixer->IsPlugin(software_mixer_plugin))
		return -1;

	return mixer->LockGetVolume();
}

int
MultipleOutputs::GetSoftwareVolume() const noexcept
{
	unsigned ok = 0;
	int total = 0;

	for (const auto &ao : outputs) {
		int volume = output_mixer_get_software_volume(*ao);
		if (volume >= 0) {
			total += volume;
			++ok;
		}
	}

	if (ok == 0)
		return -1;

	return total / ok;
}

void
MultipleOutputs::SetSoftwareVolume(unsigned volume) noexcept
{
	assert(volume <= PCM_VOLUME_1);

	for (const auto &ao : outputs) {
		auto *mixer = ao->GetMixer();

		if (mixer != nullptr &&
		    (mixer->IsPlugin(software_mixer_plugin) ||
		     mixer->IsPlugin(null_mixer_plugin)))
			mixer->LockSetVolume(volume);
	}
}
