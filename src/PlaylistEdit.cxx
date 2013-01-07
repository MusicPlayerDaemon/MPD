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
#include "Playlist.hxx"
#include "PlayerControl.hxx"

extern "C" {
#include "uri.h"
#include "song.h"
#include "idle.h"
}

#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

#include <stdlib.h>

void
playlist::OnModified()
{
	queue.IncrementVersion();

	idle_add(IDLE_PLAYLIST);
}

void
playlist::Clear(player_control &pc)
{
	Stop(pc);

	queue.Clear();
	current = -1;

	OnModified();
}

enum playlist_result
playlist::AppendFile(struct player_control &pc,
		     const char *path_fs, unsigned *added_id)
{
	struct song *song = song_file_load(path_fs, NULL);
	if (song == NULL)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return AppendSong(pc, song, added_id);
}

enum playlist_result
playlist::AppendSong(struct player_control &pc,
		     struct song *song, unsigned *added_id)
{
	unsigned id;

	if (queue.IsFull())
		return PLAYLIST_RESULT_TOO_LARGE;

	const struct song *const queued_song = GetQueuedSong();

	id = queue.Append(song, 0);

	if (queue.random) {
		/* shuffle the new song into the list of remaining
		   songs to play */

		unsigned start;
		if (queued >= 0)
			start = queued + 1;
		else
			start = current + 1;
		if (start < queue.GetLength())
			queue.ShuffleOrderLast(start, queue.GetLength());
	}

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	if (added_id)
		*added_id = id;

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist::AppendURI(struct player_control &pc,
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

	enum playlist_result result = AppendSong(pc, song, added_id);
	if (db != nullptr)
		db->ReturnSong(song);

	return result;
}

enum playlist_result
playlist::SwapPositions(player_control &pc, unsigned song1, unsigned song2)
{
	if (!queue.IsValidPosition(song1) || !queue.IsValidPosition(song2))
		return PLAYLIST_RESULT_BAD_RANGE;

	const struct song *const queued_song = GetQueuedSong();

	queue.SwapPositions(song1, song2);

	if (queue.random) {
		/* update the queue order, so that current
		   still points to the current song order */

		queue.SwapOrders(queue.PositionToOrder(song1),
				 queue.PositionToOrder(song2));
	} else {
		/* correct the "current" song order */

		if (current == (int)song1)
			current = song2;
		else if (current == (int)song2)
			current = song1;
	}

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist::SwapIds(player_control &pc, unsigned id1, unsigned id2)
{
	int song1 = queue.IdToPosition(id1);
	int song2 = queue.IdToPosition(id2);

	if (song1 < 0 || song2 < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return SwapPositions(pc, song1, song2);
}

enum playlist_result
playlist::SetPriorityRange(player_control &pc,
			   unsigned start, unsigned end,
			   uint8_t priority)
{
	if (start >= GetLength())
		return PLAYLIST_RESULT_BAD_RANGE;

	if (end > GetLength())
		end = GetLength();

	if (start >= end)
		return PLAYLIST_RESULT_SUCCESS;

	/* remember "current" and "queued" */

	const int current_position = GetCurrentPosition();
	const struct song *const queued_song = GetQueuedSong();

	/* apply the priority changes */

	queue.SetPriorityRange(start, end, priority, current);

	/* restore "current" and choose a new "queued" */

	if (current_position >= 0)
		current = queue.PositionToOrder(current_position);

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist::SetPriorityId(struct player_control &pc,
			unsigned song_id, uint8_t priority)
{
	int song_position = queue.IdToPosition(song_id);
	if (song_position < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return SetPriorityRange(pc, song_position, song_position + 1,
				priority);

}

void
playlist::DeleteInternal(player_control &pc,
			 unsigned song, const struct song **queued_p)
{
	assert(song < GetLength());

	unsigned songOrder = queue.PositionToOrder(song);

	if (playing && current == (int)songOrder) {
		bool paused = pc_get_state(&pc) == PLAYER_STATE_PAUSE;

		/* the current song is going to be deleted: stop the player */

		pc_stop(&pc);
		playing = false;

		/* see which song is going to be played instead */

		current = queue.GetNextOrder(current);
		if (current == (int)songOrder)
			current = -1;

		if (current >= 0 && !paused)
			/* play the song after the deleted one */
			PlayOrder(pc, current);
		else
			/* no songs left to play, stop playback
			   completely */
			Stop(pc);

		*queued_p = NULL;
	} else if (current == (int)songOrder)
		/* there's a "current song" but we're not playing
		   currently - clear "current" */
		current = -1;

	/* now do it: remove the song */

	queue.DeletePosition(song);

	/* update the "current" and "queued" variables */

	if (current > (int)songOrder)
		current--;
}

enum playlist_result
playlist::DeletePosition(struct player_control &pc, unsigned song)
{
	if (song >= queue.GetLength())
		return PLAYLIST_RESULT_BAD_RANGE;

	const struct song *queued_song = GetQueuedSong();

	DeleteInternal(pc, song, &queued_song);

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist::DeleteRange(struct player_control &pc, unsigned start, unsigned end)
{
	if (start >= queue.GetLength())
		return PLAYLIST_RESULT_BAD_RANGE;

	if (end > queue.GetLength())
		end = queue.GetLength();

	if (start >= end)
		return PLAYLIST_RESULT_SUCCESS;

	const struct song *queued_song = GetQueuedSong();

	do {
		DeleteInternal(pc, --end, &queued_song);
	} while (end != start);

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist::DeleteId(struct player_control &pc, unsigned id)
{
	int song = queue.IdToPosition(id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return DeletePosition(pc, song);
}

void
playlist::DeleteSong(struct player_control &pc, const struct song &song)
{
	for (int i = queue.GetLength() - 1; i >= 0; --i)
		// TODO: compare URI instead of pointer
		if (&song == queue.Get(i))
			DeletePosition(pc, i);
}

enum playlist_result
playlist::MoveRange(player_control &pc, unsigned start, unsigned end, int to)
{
	if (!queue.IsValidPosition(start) || !queue.IsValidPosition(end - 1))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((to >= 0 && to + end - start - 1 >= GetLength()) ||
	    (to < 0 && unsigned(abs(to)) > GetLength()))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((int)start == to)
		/* nothing happens */
		return PLAYLIST_RESULT_SUCCESS;

	const struct song *const queued_song = GetQueuedSong();

	/*
	 * (to < 0) => move to offset from current song
	 * (-playlist.length == to) => move to position BEFORE current song
	 */
	const int currentSong = GetCurrentPosition();
	if (to < 0 && currentSong >= 0) {
		if (start <= (unsigned)currentSong && (unsigned)currentSong < end)
			/* no-op, can't be moved to offset of itself */
			return PLAYLIST_RESULT_SUCCESS;
		to = (currentSong + abs(to)) % GetLength();
		if (start < (unsigned)to)
			to--;
	}

	queue.MoveRange(start, end, to);

	if (!queue.random) {
		/* update current/queued */
		if ((int)start <= current && (unsigned)current < end)
			current += to - start;
		else if (current >= (int)end && current <= to)
			current -= end - start;
		else if (current >= to && current < (int)start)
			current += end - start;
	}

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist::MoveId(player_control &pc, unsigned id1, int to)
{
	int song = queue.IdToPosition(id1);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return MoveRange(pc, song, song + 1, to);
}

void
playlist::Shuffle(player_control &pc, unsigned start, unsigned end)
{
	if (end > GetLength())
		/* correct the "end" offset */
		end = GetLength();

	if (start + 1 >= end)
		/* needs at least two entries. */
		return;

	const struct song *const queued_song = GetQueuedSong();
	if (playing && current >= 0) {
		unsigned current_position = queue.OrderToPosition(current);

		if (current_position >= start && current_position < end) {
			/* put current playing song first */
			queue.SwapPositions(start, current_position);

			if (queue.random) {
				current = queue.PositionToOrder(start);
			} else
				current = start;

			/* start shuffle after the current song */
			start++;
		}
	} else {
		/* no playback currently: reset current */

		current = -1;
	}

	queue.ShuffleRange(start, end);

	UpdateQueuedSong(pc, queued_song);
	OnModified();
}
