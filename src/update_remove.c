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

#include "config.h" /* must be first for large file support */
#include "update_internal.h"
#include "notify.h"
#include "event_pipe.h"
#include "song.h"
#include "playlist.h"

#ifdef ENABLE_SQLITE
#include "sticker.h"
#include "song_sticker.h"
#endif

#include <glib.h>

#include <assert.h>

static const struct song *removed_song;

static struct notify remove_notify;

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
	g_debug("removing: %s", uri);
	g_free(uri);

#ifdef ENABLE_SQLITE
	/* if the song has a sticker, remove it */
	if (sticker_enabled())
		sticker_song_delete(removed_song);
#endif

	playlist_delete_song(&g_playlist, removed_song);
	removed_song = NULL;

	notify_signal(&remove_notify);
}

void
update_remove_global_init(void)
{
	notify_init(&remove_notify);

	event_pipe_register(PIPE_EVENT_DELETE, song_remove_event);
}

void
update_remove_global_finish(void)
{
	notify_deinit(&remove_notify);
}

void
update_remove_song(const struct song *song)
{
	assert(removed_song == NULL);

	removed_song = song;

	event_pipe_emit(PIPE_EVENT_DELETE);

	do {
		notify_wait(&remove_notify);
	} while (removed_song != NULL);

}
