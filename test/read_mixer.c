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
#include "mixer_list.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, G_GNUC_UNUSED char **argv)
{
	struct mixer *mixer;
	bool success;
	int volume;

	if (argc != 2) {
		g_printerr("Usage: read_mixer PLUGIN\n");
		return 1;
	}

	g_thread_init(NULL);

	mixer = mixer_new(&alsa_mixer, NULL);
	if (mixer == NULL) {
		g_printerr("mixer_new() failed\n");
		return 2;
	}

	success = mixer_open(mixer);
	if (!success) {
		mixer_free(mixer);
		g_printerr("failed to open the mixer\n");
		return 2;
	}

	volume = mixer_get_volume(mixer);
	mixer_close(mixer);
	mixer_free(mixer);

	assert(volume >= -1 && volume <= 100);

	if (volume < 0) {
		g_printerr("failed to read volume\n");
		return 2;
	}

	g_print("%d\n", volume);
	return 0;
}
