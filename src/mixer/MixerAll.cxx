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
#include "output/MultipleOutputs.hxx"
#include "MixerControl.hxx"
#include "MixerInternal.hxx"
#include "MixerList.hxx"
#include "output/Internal.hxx"
#include "pcm/Volume.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>

static constexpr Domain mixer_domain("mixer");

static int
output_mixer_get_volume(const AudioOutput &ao)
{
	if (!ao.enabled)
		return -1;

	Mixer *mixer = ao.mixer;
	if (mixer == nullptr)
		return -1;

	Error error;
	int volume = mixer_get_volume(mixer, error);
	if (volume < 0 && error.IsDefined())
		FormatError(error,
			    "Failed to read mixer for '%s'",
			    ao.name);

	return volume;
}

int
MultipleOutputs::GetVolume() const
{
	unsigned ok = 0;
	int total = 0;

	for (auto ao : outputs) {
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
output_mixer_set_volume(AudioOutput &ao, unsigned volume)
{
	assert(volume <= 100);

	if (!ao.enabled)
		return false;

	Mixer *mixer = ao.mixer;
	if (mixer == nullptr)
		return false;

	Error error;
	bool success = mixer_set_volume(mixer, volume, error);
	if (!success && error.IsDefined())
		FormatError(error,
			    "Failed to set mixer for '%s'",
			    ao.name);

	return success;
}

bool
MultipleOutputs::SetVolume(unsigned volume)
{
	assert(volume <= 100);

	bool success = false;
	for (auto ao : outputs)
		success = output_mixer_set_volume(*ao, volume)
			|| success;

	return success;
}

static int
output_mixer_get_software_volume(const AudioOutput &ao)
{
	if (!ao.enabled)
		return -1;

	Mixer *mixer = ao.mixer;
	if (mixer == nullptr || !mixer->IsPlugin(software_mixer_plugin))
		return -1;

	return mixer_get_volume(mixer, IgnoreError());
}

int
MultipleOutputs::GetSoftwareVolume() const
{
	unsigned ok = 0;
	int total = 0;

	for (auto ao : outputs) {
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
MultipleOutputs::SetSoftwareVolume(unsigned volume)
{
	assert(volume <= PCM_VOLUME_1);

	for (auto ao : outputs) {
		const auto mixer = ao->mixer;

		if (mixer != nullptr &&
		    &mixer->plugin == &software_mixer_plugin)
			mixer_set_volume(mixer, volume, IgnoreError());
	}
}
