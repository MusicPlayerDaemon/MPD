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

#include "update_internal.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

/* make this dynamic?, or maybe this is big enough... */
static char *update_paths[32];
static size_t update_paths_nr;

unsigned
update_queue_push(const char *path, unsigned base)
{
	assert(update_paths_nr <= G_N_ELEMENTS(update_paths));

	if (update_paths_nr == G_N_ELEMENTS(update_paths))
		return 0;

	update_paths[update_paths_nr++] = g_strdup(path);
	return base + update_paths_nr;
}

char *
update_queue_shift(void)
{
	char *path;

	if (update_paths_nr == 0)
		return NULL;

	path = update_paths[0];
	memmove(&update_paths[0], &update_paths[1],
		--update_paths_nr * sizeof(char *));
	return path;
}
