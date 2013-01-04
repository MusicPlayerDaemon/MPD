/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Playlist.hxx"
#include "Main.hxx"
#include "Partition.hxx"

extern "C" {
#include "event_pipe.h"
}

static void
playlist_tag_event(void)
{
	playlist_tag_changed(&global_partition->playlist);
}

static void
playlist_event(void)
{
	playlist_sync(&global_partition->playlist,
		      &global_partition->pc);
}

void
playlist_global_init()
{
	event_pipe_register(PIPE_EVENT_TAG, playlist_tag_event);
	event_pipe_register(PIPE_EVENT_PLAYLIST, playlist_event);
}
