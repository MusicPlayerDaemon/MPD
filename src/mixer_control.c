/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "mixer_control.h"
#include "mixer_api.h"
#include "output_all.h"
#include "output_plugin.h"
#include "output_internal.h"

#include <assert.h>

bool
mixer_control_setvol(unsigned int device, int volume, int rel)
{
	struct audio_output *output;
	struct mixer *mixer;

	assert(device < audio_output_count());

	output = audio_output_get(device);
	if (!output->enabled)
		return false;

	mixer = ao_plugin_get_mixer(output->plugin, output->data);
	if (mixer != NULL) {
		if (rel) {
			int cur_volume = mixer_get_volume(mixer);
			if (cur_volume < 0)
				return false;

			volume = volume + cur_volume;
		}
		if (volume > 100)
			volume = 100;
		else if (volume < 0)
			volume = 0;

		return mixer_set_volume(mixer, volume);
	}
	return false;
}

bool
mixer_control_getvol(unsigned int device, int *volume)
{
	struct audio_output *output;
	struct mixer *mixer;

	assert(device < audio_output_count());

	output = audio_output_get(device);
	if (!output->enabled)
		return false;

	mixer = ao_plugin_get_mixer(output->plugin, output->data);
	if (mixer != NULL) {
		int volume2;

		volume2 = mixer_get_volume(mixer);
		if (volume2 < 0)
			return false;

		*volume = volume2;
		return true;
	}

	return false;
}
