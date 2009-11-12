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
 * The manager of the global "struct playlist" instance (g_playlist).
 *
 */

#include "config.h"
#include "playlist.h"
#include "playlist_state.h"
#include "event_pipe.h"

struct playlist g_playlist;

static void
playlist_tag_event(void)
{
	playlist_tag_changed(&g_playlist);
}

static void
playlist_event(void)
{
	playlist_sync(&g_playlist);
}

void
playlist_global_init(void)
{
	playlist_init(&g_playlist);

	event_pipe_register(PIPE_EVENT_TAG, playlist_tag_event);
	event_pipe_register(PIPE_EVENT_PLAYLIST, playlist_event);
}

void
playlist_global_finish(void)
{
	playlist_finish(&g_playlist);
}
