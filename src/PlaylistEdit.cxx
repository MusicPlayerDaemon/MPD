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
 * Functions for editing the playlist (adding, removing, reordering
 * songs in the queue).
 *
 */

#include "config.h"
#include "PlaylistInternal.hxx"
#include "PlayerControl.hxx"

extern "C" {
#include "uri.h"
#include "song.h"
#include "idle.h"
}

#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

#include <stdlib.h>

static void playlist_increment_version(struct playlist *playlist)
{
	playlist->queue.IncrementVersion();

	idle_add(IDLE_PLAYLIST);
}

void
playlist_clear(struct playlist *playlist, struct player_control *pc)
{
	playlist_stop(playlist, pc);

	playlist->queue.Clear();

	playlist->current = -1;

	playlist_increment_version(playlist);
}

enum playlist_result
playlist_append_file(struct playlist *playlist, struct player_control *pc,
		     const char *path_fs, unsigned *added_id)
{
	struct song *song = song_file_load(path_fs, NULL);
	if (song == NULL)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playlist_append_song(playlist, pc, song, added_id);
}

enum playlist_result
playlist_append_song(struct playlist *playlist, struct player_control *pc,
		  struct song *song, unsigned *added_id)
{
	const struct song *queued;
	unsigned id;

	if (playlist->queue.IsFull())
		return PLAYLIST_RESULT_TOO_LARGE;

	queued = playlist_get_queued_song(playlist);

	id = playlist->queue.Append(song, 0);

	if (playlist->queue.random) {
		/* shuffle the new song into the list of remaining
		   songs to play */

		unsigned start;
		if (playlist->queued >= 0)
			start = playlist->queued + 1;
		else
			start = playlist->current + 1;
		if (start < playlist->queue.GetLength())
			playlist->queue.ShuffleOrderLast(start,
							 playlist->queue.GetLength());
	}

	playlist_increment_version(playlist);

	playlist_update_queued_song(playlist, pc, queued);

	if (added_id)
		*added_id = id;

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist_append_uri(struct playlist *playlist, struct player_control *pc,
		    const char *uri, unsigned *added_id)
{
	g_debug("add to playlist: %s", uri);

	const Database *db = nullptr;
	struct song *song;
	if (uri_has_scheme(uri)) {
		song = song_remote_new(uri);
	} else {
		db = GetDatabase(nullptr);
		if (db == nullptr)
			return PLAYLIST_RESULT_NO_SUCH_SONG;

		song = db->GetSong(uri, nullptr);
		if (song == nullptr)
			return PLAYLIST_RESULT_NO_SUCH_SONG;
	}

	enum playlist_result result =
		playlist_append_song(playlist, pc, song, added_id);
	if (db != nullptr)
		db->ReturnSong(song);

	return result;
}

enum playlist_result
playlist_swap_songs(struct playlist *playlist, struct player_control *pc,
		    unsigned song1, unsigned song2)
{
	const struct song *queued;

	if (!playlist->queue.IsValidPosition(song1) ||
	    !playlist->queue.IsValidPosition(song2))
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song(playlist);

	playlist->queue.SwapPositions(song1, song2);

	if (playlist->queue.random) {
		/* update the queue order, so that playlist->current
		   still points to the current song order */

		playlist->queue.SwapOrders(playlist->queue.PositionToOrder(song1),
					   playlist->queue.PositionToOrder(song2));
	} else {
		/* correct the "current" song order */

		if (playlist->current == (int)song1)
			playlist->current = song2;
		else if (playlist->current == (int)song2)
			playlist->current = song1;
	}

	playlist_increment_version(playlist);

	playlist_update_queued_song(playlist, pc, queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist_swap_songs_id(struct playlist *playlist, struct player_control *pc,
		       unsigned id1, unsigned id2)
{
	int song1 = playlist->queue.IdToPosition(id1);
	int song2 = playlist->queue.IdToPosition(id2);

	if (song1 < 0 || song2 < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playlist_swap_songs(playlist, pc, song1, song2);
}

enum playlist_result
playlist_set_priority(struct playlist *playlist, struct player_control *pc,
		      unsigned start, unsigned end,
		      uint8_t priority)
{
	if (start >= playlist->queue.GetLength())
		return PLAYLIST_RESULT_BAD_RANGE;

	if (end > playlist->queue.GetLength())
		end = playlist->queue.GetLength();

	if (start >= end)
		return PLAYLIST_RESULT_SUCCESS;

	/* remember "current" and "queued" */

	int current_position = playlist->current >= 0
		? (int)playlist->queue.OrderToPosition(playlist->current)
		: -1;

	const struct song *queued = playlist_get_queued_song(playlist);

	/* apply the priority changes */

	playlist->queue.SetPriorityRange(start, end, priority,
					 playlist->current);

	playlist_increment_version(playlist);

	/* restore "current" and choose a new "queued" */

	if (current_position >= 0)
		playlist->current = playlist->queue.PositionToOrder(current_position);

	playlist_update_queued_song(playlist, pc, queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist_set_priority_id(struct playlist *playlist, struct player_control *pc,
			 unsigned song_id, uint8_t priority)
{
	int song_position = playlist->queue.IdToPosition(song_id);
	if (song_position < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playlist_set_priority(playlist, pc,
				     song_position, song_position + 1,
				     priority);

}

static void
playlist_delete_internal(struct playlist *playlist, struct player_control *pc,
			 unsigned song, const struct song **queued_p)
{
	unsigned songOrder;

	assert(song < playlist->queue.GetLength());

	songOrder = playlist->queue.PositionToOrder(song);

	if (playlist->playing && playlist->current == (int)songOrder) {
		bool paused = pc_get_state(pc) == PLAYER_STATE_PAUSE;

		/* the current song is going to be deleted: stop the player */

		pc_stop(pc);
		playlist->playing = false;

		/* see which song is going to be played instead */

		playlist->current = playlist->queue.GetNextOrder(playlist->current);
		if (playlist->current == (int)songOrder)
			playlist->current = -1;

		if (playlist->current >= 0 && !paused)
			/* play the song after the deleted one */
			playlist_play_order(playlist, pc, playlist->current);
		else
			/* no songs left to play, stop playback
			   completely */
			playlist_stop(playlist, pc);

		*queued_p = NULL;
	} else if (playlist->current == (int)songOrder)
		/* there's a "current song" but we're not playing
		   currently - clear "current" */
		playlist->current = -1;

	/* now do it: remove the song */

	playlist->queue.DeletePosition(song);

	/* update the "current" and "queued" variables */

	if (playlist->current > (int)songOrder) {
		playlist->current--;
	}
}

enum playlist_result
playlist_delete(struct playlist *playlist, struct player_control *pc,
		unsigned song)
{
	const struct song *queued;

	if (song >= playlist->queue.GetLength())
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song(playlist);

	playlist_delete_internal(playlist, pc, song, &queued);

	playlist_increment_version(playlist);
	playlist_update_queued_song(playlist, pc, queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist_delete_range(struct playlist *playlist, struct player_control *pc,
		      unsigned start, unsigned end)
{
	const struct song *queued;

	if (start >= playlist->queue.GetLength())
		return PLAYLIST_RESULT_BAD_RANGE;

	if (end > playlist->queue.GetLength())
		end = playlist->queue.GetLength();

	if (start >= end)
		return PLAYLIST_RESULT_SUCCESS;

	queued = playlist_get_queued_song(playlist);

	do {
		playlist_delete_internal(playlist, pc, --end, &queued);
	} while (end != start);

	playlist_increment_version(playlist);
	playlist_update_queued_song(playlist, pc, queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist_delete_id(struct playlist *playlist, struct player_control *pc,
		   unsigned id)
{
	int song = playlist->queue.IdToPosition(id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playlist_delete(playlist, pc, song);
}

void
playlist_delete_song(struct playlist *playlist, struct player_control *pc,
		     const struct song *song)
{
	for (int i = playlist->queue.GetLength() - 1; i >= 0; --i)
		if (song == playlist->queue.Get(i))
			playlist_delete(playlist, pc, i);
}

enum playlist_result
playlist_move_range(struct playlist *playlist, struct player_control *pc,
		    unsigned start, unsigned end, int to)
{
	const struct song *queued;
	int currentSong;

	if (!playlist->queue.IsValidPosition(start) ||
	    !playlist->queue.IsValidPosition(end - 1))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((to >= 0 && to + end - start - 1 >= playlist->queue.GetLength()) ||
	    (to < 0 && abs(to) > (int)playlist->queue.GetLength()))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((int)start == to)
		/* nothing happens */
		return PLAYLIST_RESULT_SUCCESS;

	queued = playlist_get_queued_song(playlist);

	/*
	 * (to < 0) => move to offset from current song
	 * (-playlist.length == to) => move to position BEFORE current song
	 */
	currentSong = playlist->current >= 0
		? (int)playlist->queue.OrderToPosition(playlist->current)
		: -1;
	if (to < 0 && playlist->current >= 0) {
		if (start <= (unsigned)currentSong && (unsigned)currentSong < end)
			/* no-op, can't be moved to offset of itself */
			return PLAYLIST_RESULT_SUCCESS;
		to = (currentSong + abs(to)) % playlist->queue.GetLength();
		if (start < (unsigned)to)
			to--;
	}

	playlist->queue.MoveRange(start, end, to);

	if (!playlist->queue.random) {
		/* update current/queued */
		if ((int)start <= playlist->current &&
		    (unsigned)playlist->current < end)
			playlist->current += to - start;
		else if (playlist->current >= (int)end &&
			 playlist->current <= to) {
			playlist->current -= end - start;
		} else if (playlist->current >= to &&
			   playlist->current < (int)start) {
			playlist->current += end - start;
		}
	}

	playlist_increment_version(playlist);

	playlist_update_queued_song(playlist, pc, queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist_move_id(struct playlist *playlist, struct player_control *pc,
		 unsigned id1, int to)
{
	int song = playlist->queue.IdToPosition(id1);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playlist_move_range(playlist, pc, song, song+1, to);
}

void
playlist_shuffle(struct playlist *playlist, struct player_control *pc,
		 unsigned start, unsigned end)
{
	const struct song *queued;

	if (end > playlist->queue.GetLength())
		/* correct the "end" offset */
		end = playlist->queue.GetLength();

	if ((start+1) >= end)
		/* needs at least two entries. */
		return;

	queued = playlist_get_queued_song(playlist);
	if (playlist->playing && playlist->current >= 0) {
		unsigned current_position;
		current_position = playlist->queue.OrderToPosition(playlist->current);

		if (current_position >= start && current_position < end)
		{
			/* put current playing song first */
			playlist->queue.SwapPositions(start, current_position);

			if (playlist->queue.random) {
				playlist->current =
					playlist->queue.PositionToOrder(start);
			} else
				playlist->current = start;

			/* start shuffle after the current song */
			start++;
		}
	} else {
		/* no playback currently: reset playlist->current */

		playlist->current = -1;
	}

	playlist->queue.ShuffleRange(start, end);

	playlist_increment_version(playlist);

	playlist_update_queued_song(playlist, pc, queued);
}
