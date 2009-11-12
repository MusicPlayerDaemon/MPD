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

#include "config.h"
#include "update_internal.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

/* make this dynamic?, or maybe this is big enough... */
static struct {
	char *path;
	bool discard;
} update_queue[32];

static size_t update_queue_length;

unsigned
update_queue_push(const char *path, bool discard, unsigned base)
{
	assert(update_queue_length <= G_N_ELEMENTS(update_queue));

	if (update_queue_length == G_N_ELEMENTS(update_queue))
		return 0;

	update_queue[update_queue_length].path = g_strdup(path);
	update_queue[update_queue_length].discard = discard;

	++update_queue_length;

	return base + update_queue_length;
}

char *
update_queue_shift(bool *discard_r)
{
	char *path;

	if (update_queue_length == 0)
		return NULL;

	path = update_queue[0].path;
	*discard_r = update_queue[0].discard;

	memmove(&update_queue[0], &update_queue[1],
		--update_queue_length * sizeof(update_queue[0]));
	return path;
}
