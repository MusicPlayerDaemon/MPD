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

/*
 * Saving and loading the audio output states to/from the state file.
 *
 */

#include "config.h"
#include "output_state.h"
#include "output_internal.h"
#include "output_all.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_DEVICE_STATE "audio_device_state:"

unsigned audio_output_state_version;

void
audio_output_state_save(FILE *fp)
{
	unsigned n = audio_output_count();

	assert(n > 0);

	for (unsigned i = 0; i < n; ++i) {
		const struct audio_output *ao = audio_output_get(i);

		fprintf(fp, AUDIO_DEVICE_STATE "%d:%s\n",
			ao->enabled, ao->name);
	}
}

bool
audio_output_state_read(const char *line)
{
	long value;
	char *endptr;
	const char *name;
	struct audio_output *ao;

	if (!g_str_has_prefix(line, AUDIO_DEVICE_STATE))
		return false;

	line += sizeof(AUDIO_DEVICE_STATE) - 1;

	value = strtol(line, &endptr, 10);
	if (*endptr != ':' || (value != 0 && value != 1))
		return false;

	if (value != 0)
		/* state is "enabled": no-op */
		return true;

	name = endptr + 1;
	ao = audio_output_find(name);
	if (ao == NULL) {
		g_debug("Ignoring device state for '%s'", name);
		return true;
	}

	ao->enabled = false;
	return true;
}

unsigned
audio_output_state_get_version(void)
{
	return audio_output_state_version;
}
