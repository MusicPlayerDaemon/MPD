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
 * Functions for controlling playback on the playlist level.
 *
 */

#include "config.h"
#include "PlaylistInternal.hxx"
#include "PlayerControl.hxx"
#include "song.h"

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
			playlist->queue.OrderToPosition(playlist->current);

		playlist->queue.ShuffleOrder();

		/* make sure that "current" stays valid, and the next
		   "play" command plays the same song again */
		playlist->current =
			playlist->queue.PositionToOrder(current_position);
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

		if (playlist->queue.IsEmpty())
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
	} else if (!playlist->queue.IsValidPosition(song))
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist->queue.random) {
		if (song >= 0)
			/* "i" is currently the song position (which
			   would be equal to the order number in
			   no-random mode); convert it to a order
			   number, because random mode is enabled */
			i = playlist->queue.PositionToOrder(song);

		if (!playlist->playing)
			playlist->current = 0;

		/* swap the new song with the previous "current" one,
		   so playback continues as planned */
		playlist->queue.SwapOrders(i, playlist->current);
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

	song = playlist->queue.IdToPosition(id);
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

	assert(!playlist->queue.IsEmpty());
	assert(playlist->queue.IsValidOrder(playlist->current));

	current = playlist->current;
	playlist->stop_on_error = false;

	/* determine the next song from the queue's order list */

	next_order = playlist->queue.GetNextOrder(playlist->current);
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

			playlist->queue.ShuffleOrder();

			/* note that playlist->current and playlist->queued are
			   now invalid, but playlist_play_order() will
			   discard them anyway */
		}

		playlist_play_order(playlist, pc, next_order);
	}

	/* Consume mode removes each played songs. */
	if(playlist->queue.consume)
		playlist_delete(playlist, pc,
				playlist->queue.OrderToPosition(current));
}

void
playlist_previous(struct playlist *playlist, struct player_control *pc)
{
	if (!playlist->playing)
		return;

	assert(playlist->queue.GetLength() > 0);

	if (playlist->current > 0) {
		/* play the preceding song */
		playlist_play_order(playlist, pc,
				    playlist->current - 1);
	} else if (playlist->queue.repeat) {
		/* play the last song in "repeat" mode */
		playlist_play_order(playlist, pc,
				    playlist->queue.GetLength() - 1);
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

	if (!playlist->queue.IsValidPosition(song))
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song(playlist);

	if (playlist->queue.random)
		i = playlist->queue.PositionToOrder(song);
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

	struct song *the_song =
		song_dup_detached(playlist->queue.GetOrder(i));
	success = pc_seek(pc, the_song, seek_time);
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
	int song = playlist->queue.IdToPosition(id);
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
