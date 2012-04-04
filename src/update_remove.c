/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#include "config.h" /* must be first for large file support */
#include "update_remove.h"
#include "event_pipe.h"
#include "song.h"
#include "playlist.h"
#include "main.h"

#ifdef ENABLE_SQLITE
#include "sticker.h"
#include "song_sticker.h"
#endif

#include <glib.h>

#include <assert.h>

static const struct song *removed_song;

static GMutex *remove_mutex;
static GCond *remove_cond;

/**
 * Safely remove a song from the database.  This must be done in the
 * main task, to be sure that there is no pointer left to it.
 */
static void
song_remove_event(void)
{
	char *uri;

	assert(removed_song != NULL);

	uri = song_get_uri(removed_song);
	g_message("removing %s", uri);
	g_free(uri);

#ifdef ENABLE_SQLITE
	/* if the song has a sticker, remove it */
	if (sticker_enabled())
		sticker_song_delete(removed_song);
#endif

	playlist_delete_song(&g_playlist, global_player_control, removed_song);

	/* clear "removed_song" and send signal to update thread */
	g_mutex_lock(remove_mutex);
	removed_song = NULL;
	g_cond_signal(remove_cond);
	g_mutex_unlock(remove_mutex);
}

void
update_remove_global_init(void)
{
	remove_mutex = g_mutex_new();
	remove_cond = g_cond_new();

	event_pipe_register(PIPE_EVENT_DELETE, song_remove_event);
}

void
update_remove_global_finish(void)
{
	g_mutex_free(remove_mutex);
	g_cond_free(remove_cond);
}

void
update_remove_song(const struct song *song)
{
	assert(removed_song == NULL);

	removed_song = song;

	event_pipe_emit(PIPE_EVENT_DELETE);

	g_mutex_lock(remove_mutex);

	while (removed_song != NULL)
		g_cond_wait(remove_cond, remove_mutex);

	g_mutex_unlock(remove_mutex);
}
