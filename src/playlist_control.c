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
 * Functions for controlling playback on the playlist level.
 *
 */

#include "playlist_internal.h"
#include "player_control.h"
#include "idle.h"

#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "playlist"

enum {
	/**
	 * When the "prev" command is received, rewind the current
	 * track if this number of seconds has already elapsed.
	 */
	PLAYLIST_PREV_UNLESS_ELAPSED = 10,
};

void stopPlaylist(struct playlist *playlist)
{
	if (!playlist->playing)
		return;

	assert(playlist->current >= 0);

	g_debug("stop");
	playerWait();
	playlist->queued = -1;
	playlist->playing = false;

	if (playlist->queue.random) {
		/* shuffle the playlist, so the next playback will
		   result in a new random order */

		unsigned current_position =
			queue_order_to_position(&playlist->queue,
						playlist->current);

		queue_shuffle_order(&playlist->queue);

		/* make sure that "current" stays valid, and the next
		   "play" command plays the same song again */
		playlist->current =
			queue_position_to_order(&playlist->queue,
						current_position);
	}
}

enum playlist_result playPlaylist(struct playlist *playlist, int song)
{
	unsigned i = song;

	clearPlayerError();

	if (song == -1) {
		/* play any song ("current" song, or the first song */

		if (queue_is_empty(&playlist->queue))
			return PLAYLIST_RESULT_SUCCESS;

		if (playlist->playing) {
			/* already playing: unpause playback, just in
			   case it was paused, and return */
			playerSetPause(0);
			return PLAYLIST_RESULT_SUCCESS;
		}

		/* select a song: "current" song, or the first one */
		i = playlist->current >= 0
			? playlist->current
			: 0;
	} else if (!queue_valid_position(&playlist->queue, song))
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist->queue.random) {
		if (song >= 0)
			/* "i" is currently the song position (which
			   would be equal to the order number in
			   no-random mode); convert it to a order
			   number, because random mode is enabled */
			i = queue_position_to_order(&playlist->queue, song);

		if (!playlist->playing)
			playlist->current = 0;

		/* swap the new song with the previous "current" one,
		   so playback continues as planned */
		queue_swap_order(&playlist->queue,
				 i, playlist->current);
		i = playlist->current;
	}

	playlist->stop_on_error = false;
	playlist->error_count = 0;

	playPlaylistOrderNumber(playlist, i);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playPlaylistById(struct playlist *playlist, int id)
{
	int song;

	if (id == -1) {
		return playPlaylist(playlist, id);
	}

	song = queue_id_to_position(&playlist->queue, id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playPlaylist(playlist, song);
}

void
nextSongInPlaylist(struct playlist *playlist)
{
	int next_order;
	int current;

	if (!playlist->playing)
		return;

	assert(!queue_is_empty(&playlist->queue));
	assert(queue_valid_order(&playlist->queue, playlist->current));

	current = playlist->current;
	playlist->stop_on_error = false;

	/* determine the next song from the queue's order list */

	next_order = queue_next_order(&playlist->queue, playlist->current);
	if (next_order < 0) {
		/* cancel single */
		playlist->queue.single = false;
		idle_add(IDLE_OPTIONS);

		/* no song after this one: stop playback */
		stopPlaylist(playlist);

		/* reset "current song" */
		playlist->current = -1;
	}
	else
	{
		if (next_order == 0 && playlist->queue.random) {
			/* The queue told us that the next song is the first
			   song.  This means we are in repeat mode.  Shuffle
			   the queue order, so this time, the user hears the
			   songs in a different than before */
			assert(playlist->queue.repeat);

			queue_shuffle_order(&playlist->queue);

			/* note that playlist->current and playlist->queued are
			   now invalid, but playPlaylistOrderNumber() will
			   discard them anyway */
		}

		playPlaylistOrderNumber(playlist, next_order);
	}

	/* Consume mode removes each played songs. */
	if(playlist->queue.consume)
		deleteFromPlaylist(playlist, queue_order_to_position(&playlist->queue, current));
}

void previousSongInPlaylist(struct playlist *playlist)
{
	if (!playlist->playing)
		return;

	if (g_timer_elapsed(playlist->prev_elapsed, NULL) >= 1.0 &&
	    getPlayerElapsedTime() > PLAYLIST_PREV_UNLESS_ELAPSED) {
		/* re-start playing the current song (just like the
		   "prev" button on CD players) */

		playPlaylistOrderNumber(playlist, playlist->current);
	} else {
		if (playlist->current > 0) {
			/* play the preceding song */
			playPlaylistOrderNumber(playlist,
						playlist->current - 1);
		} else if (playlist->queue.repeat) {
			/* play the last song in "repeat" mode */
			playPlaylistOrderNumber(playlist,
						queue_length(&playlist->queue) - 1);
		} else {
			/* re-start playing the current song if it's
			   the first one */
			playPlaylistOrderNumber(playlist, playlist->current);
		}
	}

	g_timer_start(playlist->prev_elapsed);
}

enum playlist_result
seekSongInPlaylist(struct playlist *playlist, unsigned song, float seek_time)
{
	const struct song *queued;
	unsigned i;
	bool success;

	if (!queue_valid_position(&playlist->queue, song))
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song(playlist);

	if (playlist->queue.random)
		i = queue_position_to_order(&playlist->queue, song);
	else
		i = song;

	clearPlayerError();
	playlist->stop_on_error = true;
	playlist->error_count = 0;

	if (!playlist->playing || (unsigned)playlist->current != i) {
		/* seeking is not within the current song - first
		   start playing the new song */

		playPlaylistOrderNumber(playlist, i);
		queued = NULL;
	}

	success = pc_seek(queue_get_order(&playlist->queue, i), seek_time);
	if (!success) {
		playlist_update_queued_song(playlist, queued);

		return PLAYLIST_RESULT_NOT_PLAYING;
	}

	playlist->queued = -1;
	playlist_update_queued_song(playlist, NULL);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
seekSongInPlaylistById(struct playlist *playlist, unsigned id, float seek_time)
{
	int song = queue_id_to_position(&playlist->queue, id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return seekSongInPlaylist(playlist, song, seek_time);
}
