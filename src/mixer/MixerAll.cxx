/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "output/MultipleOutputs.hxx"
#include "MixerControl.hxx"
#include "MixerInternal.hxx"
#include "MixerList.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "pcm/Volume.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <cassert>

static constexpr Domain mixer_domain("mixer");

gcc_pure
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
		return mixer_get_volume(mixer);
	} catch (...) {
		FmtError(mixer_domain,
			 "Failed to read mixer for '{}': {}",
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

static bool
output_mixer_set_volume(AudioOutputControl &ao, unsigned volume) noexcept
{
	assert(volume <= 100);

	auto *mixer = ao.GetMixer();
	if (mixer == nullptr)
		return false;

	/* software mixers are always updated, even if they are
	   disabled */
	if (!ao.IsEnabled() && !mixer->IsPlugin(software_mixer_plugin))
		return false;

	try {
		mixer_set_volume(mixer, volume);
		return true;
	} catch (...) {
		FmtError(mixer_domain,
			 "Failed to set mixer for '{}': {}",
			 ao.GetName(), std::current_exception());
		return false;
	}
}

bool
MultipleOutputs::SetVolume(unsigned volume) noexcept
{
	assert(volume <= 100);

	bool success = false;
	for (const auto &ao : outputs)
		success = output_mixer_set_volume(*ao, volume)
			|| success;

	return success;
}

static int
output_mixer_get_software_volume(const AudioOutputControl &ao) noexcept
{
	if (!ao.IsEnabled())
		return -1;

	auto *mixer = ao.GetMixer();
	if (mixer == nullptr || !mixer->IsPlugin(software_mixer_plugin))
		return -1;

	return mixer_get_volume(mixer);
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
		    (&mixer->plugin == &software_mixer_plugin ||
		     &mixer->plugin == &null_mixer_plugin))
			mixer_set_volume(mixer, volume);
	}
}
