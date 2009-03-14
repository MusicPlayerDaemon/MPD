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

#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "mixer"

int
mixer_all_get_volume(void)
{
	unsigned count = audio_output_count(), ok = 0;
	int volume, total = 0;

	for (unsigned i = 0; i < count; i++) {
		if (mixer_control_getvol(i, &volume)) {
			g_debug("device %d: volume=%d", i, volume);
			total += volume;
			++ok;
		}
	}

	if (ok == 0)
		return -1;

	return total / ok;
}

bool
mixer_all_set_volume(int volume, bool relative)
{
	bool success = false;
	unsigned count = audio_output_count();

	for (unsigned i = 0; i < count; i++)
		success = mixer_control_setvol(i, volume, relative) || success;

	return success;
}
