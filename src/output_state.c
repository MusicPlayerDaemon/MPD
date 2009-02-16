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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Saving and loading the audio output states to/from the state file.
 *
 */

#include "output_state.h"
#include "output_internal.h"
#include "output_all.h"

#include <glib.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_DEVICE_STATE "audio_device_state:"

void
saveAudioDevicesState(FILE *fp)
{
	unsigned n = audio_output_count();

	assert(n > 0);

	for (unsigned i = 0; i < n; ++i) {
		const struct audio_output *ao = audio_output_get(i);

		fprintf(fp, AUDIO_DEVICE_STATE "%d:%s\n",
			ao->enabled, ao->name);
	}
}

void
readAudioDevicesState(FILE *fp)
{
	char buffer[1024];

	while (fgets(buffer, sizeof(buffer), fp)) {
		char *c, *name;
		struct audio_output *ao;

		g_strchomp(buffer);

		if (!g_str_has_prefix(buffer, AUDIO_DEVICE_STATE))
			continue;

		c = strchr(buffer, ':');
		if (!c || !(++c))
			goto errline;

		name = strchr(c, ':');
		if (!name || !(++name))
			goto errline;

		ao = audio_output_find(name);
		if (ao != NULL && atoi(c) == 0)
			ao->enabled = false;

		continue;
errline:
		/* nonfatal */
		g_warning("invalid line in state_file: %s\n", buffer);
	}
}
