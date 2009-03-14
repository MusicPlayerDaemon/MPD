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

#include "mixer_all.h"
#include "mixer_control.h"
#include "output_all.h"
#include "output_plugin.h"
#include "output_internal.h"

#include <glib.h>

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mixer"

static int
output_mixer_get_volume(unsigned i)
{
	struct audio_output *output;
	struct mixer *mixer;

	assert(i < audio_output_count());

	output = audio_output_get(i);
	if (!output->enabled)
		return -1;

	mixer = ao_plugin_get_mixer(output->plugin, output->data);
	if (mixer == NULL)
		return -1;

	return mixer_get_volume(mixer);
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
output_mixer_set_volume(unsigned i, int volume, bool relative)
{
	struct audio_output *output;
	struct mixer *mixer;

	assert(i < audio_output_count());

	output = audio_output_get(i);
	if (!output->enabled)
		return false;

	mixer = ao_plugin_get_mixer(output->plugin, output->data);
	if (mixer == NULL)
		return false;

	if (relative) {
		int prev = mixer_get_volume(mixer);
		if (prev < 0)
			return false;

		volume += prev;
	}

	if (volume > 100)
		volume = 100;
	else if (volume < 0)
		volume = 0;

	return mixer_set_volume(mixer, volume);
}

bool
mixer_all_set_volume(int volume, bool relative)
{
	bool success = false;
	unsigned count = audio_output_count();

	for (unsigned i = 0; i < count; i++)
		success = output_mixer_set_volume(i, volume, relative)
			|| success;

	return success;
}
