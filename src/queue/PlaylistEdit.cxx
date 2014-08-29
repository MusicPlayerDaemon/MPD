/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "PlaylistError.hxx"
#include "PlayerControl.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "DetachedSong.hxx"
#include "SongLoader.hxx"
#include "Idle.hxx"

#include <stdlib.h>

void
playlist::OnModified()
{
	if (bulk_edit) {
		/* postponed to CommitBulk() */
		bulk_modified = true;
		return;
	}

	queue.IncrementVersion();

	idle_add(IDLE_PLAYLIST);
}

void
playlist::Clear(PlayerControl &pc)
{
	Stop(pc);

	queue.Clear();
	current = -1;

	OnModified();
}

void
playlist::BeginBulk()
{
	assert(!bulk_edit);

	bulk_edit = true;
	bulk_modified = false;
}

void
playlist::CommitBulk(PlayerControl &pc)
{
	assert(bulk_edit);

	bulk_edit = false;
	if (!bulk_modified)
		return;

	if (queued < 0)
		/* if no song was queued, UpdateQueuedSong() is being
		   ignored in "bulk" edit mode; now that we have
		   shuffled all new songs, we can pick a random one
		   (instead of always picking the first one that was
		   added) */
		UpdateQueuedSong(pc, nullptr);

	OnModified();
}

unsigned
playlist::AppendSong(PlayerControl &pc, DetachedSong &&song, Error &error)
{
	unsigned id;

	if (queue.IsFull()) {
		error.Set(playlist_domain, int(PlaylistResult::TOO_LARGE),
			  "Playlist is too large");
		return 0;
	}

	const DetachedSong *const queued_song = GetQueuedSong();

	id = queue.Append(std::move(song), 0);

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

	return id;
}

unsigned
playlist::AppendURI(PlayerControl &pc, const SongLoader &loader,
		    const char *uri,
		    Error &error)
{
	DetachedSong *song = loader.LoadSong(uri, error);
	if (song == nullptr)
		return 0;

	unsigned result = AppendSong(pc, std::move(*song), error);
	delete song;

	return result;
}

PlaylistResult
playlist::SwapPositions(PlayerControl &pc, unsigned song1, unsigned song2)
{
	if (!queue.IsValidPosition(song1) || !queue.IsValidPosition(song2))
		return PlaylistResult::BAD_RANGE;

	const DetachedSong *const queued_song = GetQueuedSong();

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

	return PlaylistResult::SUCCESS;
}

PlaylistResult
playlist::SwapIds(PlayerControl &pc, unsigned id1, unsigned id2)
{
	int song1 = queue.IdToPosition(id1);
	int song2 = queue.IdToPosition(id2);

	if (song1 < 0 || song2 < 0)
		return PlaylistResult::NO_SUCH_SONG;

	return SwapPositions(pc, song1, song2);
}

PlaylistResult
playlist::SetPriorityRange(PlayerControl &pc,
			   unsigned start, unsigned end,
			   uint8_t priority)
{
	if (start >= GetLength())
		return PlaylistResult::BAD_RANGE;

	if (end > GetLength())
		end = GetLength();

	if (start >= end)
		return PlaylistResult::SUCCESS;

	/* remember "current" and "queued" */

	const int current_position = GetCurrentPosition();
	const DetachedSong *const queued_song = GetQueuedSong();

	/* apply the priority changes */

	queue.SetPriorityRange(start, end, priority, current);

	/* restore "current" and choose a new "queued" */

	if (current_position >= 0)
		current = queue.PositionToOrder(current_position);

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	return PlaylistResult::SUCCESS;
}

PlaylistResult
playlist::SetPriorityId(PlayerControl &pc,
			unsigned song_id, uint8_t priority)
{
	int song_position = queue.IdToPosition(song_id);
	if (song_position < 0)
		return PlaylistResult::NO_SUCH_SONG;

	return SetPriorityRange(pc, song_position, song_position + 1,
				priority);

}

void
playlist::DeleteInternal(PlayerControl &pc,
			 unsigned song, const DetachedSong **queued_p)
{
	assert(song < GetLength());

	unsigned songOrder = queue.PositionToOrder(song);

	if (playing && current == (int)songOrder) {
		const bool paused = pc.GetState() == PlayerState::PAUSE;

		/* the current song is going to be deleted: see which
		   song is going to be played instead */

		current = queue.GetNextOrder(current);
		if (current == (int)songOrder)
			current = -1;

		if (current >= 0 && !paused)
			/* play the song after the deleted one */
			PlayOrder(pc, current);
		else {
			/* stop the player */

			pc.Stop();
			playing = false;
		}

		*queued_p = nullptr;
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

PlaylistResult
playlist::DeletePosition(PlayerControl &pc, unsigned song)
{
	if (song >= queue.GetLength())
		return PlaylistResult::BAD_RANGE;

	const DetachedSong *queued_song = GetQueuedSong();

	DeleteInternal(pc, song, &queued_song);

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	return PlaylistResult::SUCCESS;
}

PlaylistResult
playlist::DeleteRange(PlayerControl &pc, unsigned start, unsigned end)
{
	if (start >= queue.GetLength())
		return PlaylistResult::BAD_RANGE;

	if (end > queue.GetLength())
		end = queue.GetLength();

	if (start >= end)
		return PlaylistResult::SUCCESS;

	const DetachedSong *queued_song = GetQueuedSong();

	do {
		DeleteInternal(pc, --end, &queued_song);
	} while (end != start);

	UpdateQueuedSong(pc, queued_song);
	OnModified();

	return PlaylistResult::SUCCESS;
}

PlaylistResult
playlist::DeleteId(PlayerControl &pc, unsigned id)
{
	int song = queue.IdToPosition(id);
	if (song < 0)
		return PlaylistResult::NO_SUCH_SONG;

	return DeletePosition(pc, song);
}

void
playlist::DeleteSong(PlayerControl &pc, const char *uri)
{
	for (int i = queue.GetLength() - 1; i >= 0; --i)
		if (queue.Get(i).IsURI(uri))
			DeletePosition(pc, i);
}

PlaylistResult
playlist::MoveRange(PlayerControl &pc, unsigned start, unsigned end, int to)
{
	if (!queue.IsValidPosition(start) || !queue.IsValidPosition(end - 1))
		return PlaylistResult::BAD_RANGE;

	if ((to >= 0 && to + end - start - 1 >= GetLength()) ||
	    (to < 0 && unsigned(abs(to)) > GetLength()))
		return PlaylistResult::BAD_RANGE;

	if ((int)start == to)
		/* nothing happens */
		return PlaylistResult::SUCCESS;

	const DetachedSong *const queued_song = GetQueuedSong();

	/*
	 * (to < 0) => move to offset from current song
	 * (-playlist.length == to) => move to position BEFORE current song
	 */
	const int currentSong = GetCurrentPosition();
	if (to < 0) {
		if (currentSong < 0)
			/* can't move relative to current song,
			   because there is no current song */
			return PlaylistResult::BAD_RANGE;

		if (start <= (unsigned)currentSong && (unsigned)currentSong < end)
			/* no-op, can't be moved to offset of itself */
			return PlaylistResult::SUCCESS;
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

	return PlaylistResult::SUCCESS;
}

PlaylistResult
playlist::MoveId(PlayerControl &pc, unsigned id1, int to)
{
	int song = queue.IdToPosition(id1);
	if (song < 0)
		return PlaylistResult::NO_SUCH_SONG;

	return MoveRange(pc, song, song + 1, to);
}

void
playlist::Shuffle(PlayerControl &pc, unsigned start, unsigned end)
{
	if (end > GetLength())
		/* correct the "end" offset */
		end = GetLength();

	if (start + 1 >= end)
		/* needs at least two entries. */
		return;

	const DetachedSong *const queued_song = GetQueuedSong();
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

bool
playlist::SetSongIdRange(PlayerControl &pc, unsigned id,
			 SongTime start, SongTime end,
			 Error &error)
{
	assert(end.IsZero() || start < end);

	int position = queue.IdToPosition(id);
	if (position < 0) {
		error.Set(playlist_domain, int(PlaylistResult::NO_SUCH_SONG),
			  "No such song");
		return false;
	}

	if (playing) {
		if (position == current) {
			error.Set(playlist_domain, int(PlaylistResult::DENIED),
				  "Cannot edit the current song");
			return false;
		}

		if (position == queued) {
			/* if we're manipulating the "queued" song,
			   the decoder thread may be decoding it
			   already; cancel that */
			pc.Cancel();
			queued = -1;
		}
	}

	DetachedSong &song = queue.Get(position);

	const auto duration = song.GetTag().duration;
	if (!duration.IsNegative()) {
		/* validate the offsets */

		if (start > duration) {
			error.Set(playlist_domain,
				  int(PlaylistResult::BAD_RANGE),
				  "Invalid start offset");
			return false;
		}

		if (end >= duration)
			end = SongTime::zero();
	}

	/* edit it */
	song.SetStartTime(start);
	song.SetEndTime(end);

	/* announce the change to all interested subsystems */
	UpdateQueuedSong(pc, nullptr);
	queue.ModifyAtPosition(position);
	OnModified();
	return true;
}
