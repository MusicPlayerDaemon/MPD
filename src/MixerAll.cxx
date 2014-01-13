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
#include "MixerAll.hxx"
#include "MixerControl.hxx"
#include "MixerInternal.hxx"
#include "MixerList.hxx"
#include "OutputAll.hxx"
#include "pcm/Volume.hxx"
#include "OutputInternal.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <assert.h>

static constexpr Domain mixer_domain("mixer");

static int
output_mixer_get_volume(unsigned i)
{
	struct audio_output *output;
	int volume;

	assert(i < audio_output_count());

	output = audio_output_get(i);
	if (!output->enabled)
		return -1;

	Mixer *mixer = output->mixer;
	if (mixer == nullptr)
		return -1;

	Error error;
	volume = mixer_get_volume(mixer, error);
	if (volume < 0 && error.IsDefined())
		FormatError(error,
			    "Failed to read mixer for '%s'",
			    output->name);

	return volume;
}

int
mixer_all_get_volume(void)
{
	unsigned count = audio_output_count(), ok = 0;
	int volume, total = 0;

	for (unsigned i = 0; i < count; i++) {
		volume = output_mixer_get_volume(i);
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
output_mixer_set_volume(unsigned i, unsigned volume)
{
	struct audio_output *output;
	bool success;

	assert(i < audio_output_count());
	assert(volume <= 100);

	output = audio_output_get(i);
	if (!output->enabled)
		return false;

	Mixer *mixer = output->mixer;
	if (mixer == nullptr)
		return false;

	Error error;
	success = mixer_set_volume(mixer, volume, error);
	if (!success && error.IsDefined())
		FormatError(error,
			    "Failed to set mixer for '%s'",
			    output->name);

	return success;
}

bool
mixer_all_set_volume(unsigned volume)
{
	bool success = false;
	unsigned count = audio_output_count();

	assert(volume <= 100);

	for (unsigned i = 0; i < count; i++)
		success = output_mixer_set_volume(i, volume)
			|| success;

	return success;
}

static int
output_mixer_get_software_volume(unsigned i)
{
	struct audio_output *output;

	assert(i < audio_output_count());

	output = audio_output_get(i);
	if (!output->enabled)
		return -1;

	Mixer *mixer = output->mixer;
	if (mixer == nullptr || !mixer->IsPlugin(software_mixer_plugin))
		return -1;

	return mixer_get_volume(mixer, IgnoreError());
}

int
mixer_all_get_software_volume(void)
{
	unsigned count = audio_output_count(), ok = 0;
	int volume, total = 0;

	for (unsigned i = 0; i < count; i++) {
		volume = output_mixer_get_software_volume(i);
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
mixer_all_set_software_volume(unsigned volume)
{
	unsigned count = audio_output_count();

	assert(volume <= PCM_VOLUME_1);

	for (unsigned i = 0; i < count; i++) {
		struct audio_output *output = audio_output_get(i);
		if (output->mixer != nullptr &&
		    output->mixer->plugin == &software_mixer_plugin)
			mixer_set_volume(output->mixer, volume, IgnoreError());
	}
}
