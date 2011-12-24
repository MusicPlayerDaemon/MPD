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

/*
 * Functions for controlling playback on the playlist level.
 *
 */

#include "config.h"
#include "playlist_internal.h"
#include "player_control.h"
#include "idle.h"

#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "playlist"

void
playlist_stop(struct playlist *playlist, struct player_control *pc)
{
	if (!playlist->playing)
		return;

	assert(playlist->current >= 0);

	g_debug("stop");
	pc_stop(pc);
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

enum playlist_result
playlist_play(struct playlist *playlist, struct player_control *pc,
	      int song)
{
	unsigned i = song;

	pc_clear_error(pc);

	if (song == -1) {
		/* play any song ("current" song, or the first song */

		if (queue_is_empty(&playlist->queue))
			return PLAYLIST_RESULT_SUCCESS;

		if (playlist->playing) {
			/* already playing: unpause playback, just in
			   case it was paused, and return */
			pc_set_pause(pc, false);
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

	playlist_play_order(playlist, pc, i);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist_play_id(struct playlist *playlist, struct player_control *pc,
		 int id)
{
	int song;

	if (id == -1) {
		return playlist_play(playlist, pc, id);
	}

	song = queue_id_to_position(&playlist->queue, id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playlist_play(playlist, pc, song);
}

void
playlist_next(struct playlist *playlist, struct player_control *pc)
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
		/* no song after this one: stop playback */
		playlist_stop(playlist, pc);

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
			   now invalid, but playlist_play_order() will
			   discard them anyway */
		}

		playlist_play_order(playlist, pc, next_order);
	}

	/* Consume mode removes each played songs. */
	if(playlist->queue.consume)
		playlist_delete(playlist, pc,
				queue_order_to_position(&playlist->queue,
							current));
}

void
playlist_previous(struct playlist *playlist, struct player_control *pc)
{
	if (!playlist->playing)
		return;

	assert(queue_length(&playlist->queue) > 0);

	if (playlist->current > 0) {
		/* play the preceding song */
		playlist_play_order(playlist, pc,
				    playlist->current - 1);
	} else if (playlist->queue.repeat) {
		/* play the last song in "repeat" mode */
		playlist_play_order(playlist, pc,
				    queue_length(&playlist->queue) - 1);
	} else {
		/* re-start playing the current song if it's
		   the first one */
		playlist_play_order(playlist, pc, playlist->current);
	}
}

enum playlist_result
playlist_seek_song(struct playlist *playlist, struct player_control *pc,
		   unsigned song, float seek_time)
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

	pc_clear_error(pc);
	playlist->stop_on_error = true;
	playlist->error_count = 0;

	if (!playlist->playing || (unsigned)playlist->current != i) {
		/* seeking is not within the current song - prepare
		   song change */

		playlist->playing = true;
		playlist->current = i;

		queued = NULL;
	}

	success = pc_seek(pc, queue_get_order(&playlist->queue, i), seek_time);
	if (!success) {
		playlist_update_queued_song(playlist, pc, queued);

		return PLAYLIST_RESULT_NOT_PLAYING;
	}

	playlist->queued = -1;
	playlist_update_queued_song(playlist, pc, NULL);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist_seek_song_id(struct playlist *playlist, struct player_control *pc,
		      unsigned id, float seek_time)
{
	int song = queue_id_to_position(&playlist->queue, id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playlist_seek_song(playlist, pc, song, seek_time);
}

enum playlist_result
playlist_seek_current(struct playlist *playlist, struct player_control *pc,
		      float seek_time, bool relative)
{
	if (!playlist->playing)
		return PLAYLIST_RESULT_NOT_PLAYING;

	if (relative) {
		struct player_status status;
		pc_get_status(pc, &status);

		if (status.state != PLAYER_STATE_PLAY &&
		    status.state != PLAYER_STATE_PAUSE)
			return PLAYLIST_RESULT_NOT_PLAYING;

		seek_time += (int)status.elapsed_time;
	}

	if (seek_time < 0)
		seek_time = 0;

	return playlist_seek_song(playlist, pc, playlist->current, seek_time);
}
