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
 * Support library for the "idle" command.
 *
 */

#include "config.h"
#include "idle.h"
#include "event_pipe.h"

#include <assert.h>
#include <glib.h>

static unsigned idle_flags;
static GMutex *idle_mutex = NULL;

static const char *const idle_names[] = {
	"database",
	"stored_playlist",
	"playlist",
	"player",
	"mixer",
	"output",
	"options",
	"sticker",
	"update",
        NULL
};

void
idle_init(void)
{
	g_assert(idle_mutex == NULL);
	idle_mutex = g_mutex_new();
}

void
idle_deinit(void)
{
	g_assert(idle_mutex != NULL);
	g_mutex_free(idle_mutex);
	idle_mutex = NULL;
}

void
idle_add(unsigned flags)
{
	assert(flags != 0);

	g_mutex_lock(idle_mutex);
	idle_flags |= flags;
	g_mutex_unlock(idle_mutex);

	event_pipe_emit(PIPE_EVENT_IDLE);
}

unsigned
idle_get(void)
{
	unsigned flags;

	g_mutex_lock(idle_mutex);
	flags = idle_flags;
	idle_flags = 0;
	g_mutex_unlock(idle_mutex);

	return flags;
}

const char*const*
idle_get_names(void)
{
        return idle_names;
}
