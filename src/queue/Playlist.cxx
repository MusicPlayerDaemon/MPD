/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h"
#include "Playlist.hxx"
#include "PlaylistError.hxx"
#include "player/Control.hxx"
#include "DetachedSong.hxx"
#include "Idle.hxx"
#include "Log.hxx"

#include <assert.h>

void
playlist::TagModified(DetachedSong &&song)
{
	if (!playing)
		return;

	assert(current >= 0);

	DetachedSong &current_song = queue.GetOrder(current);
	if (song.IsSame(current_song))
		current_song.MoveTagItemsFrom(std::move(song));

	queue.ModifyAtOrder(current);
	queue.IncrementVersion();
	idle_add(IDLE_PLAYLIST);
}

inline void
playlist::QueueSongOrder(PlayerControl &pc, unsigned order)

{
	assert(queue.IsValidOrder(order));

	queued = order;

	const DetachedSong &song = queue.GetOrder(order);

	FormatDebug(playlist_domain, "queue song %i:\"%s\"",
		    queued, song.GetURI());

	pc.LockEnqueueSong(new DetachedSong(song));
}

void
playlist::SongStarted()
{
	assert(current >= 0);

	/* reset a song's "priority" when playback starts */
	if (queue.SetPriority(queue.OrderToPosition(current), 0, -1, false))
		OnModified();
}

inline void
playlist::QueuedSongStarted(PlayerControl &pc)
{
	assert(pc.next_song == nullptr);
	assert(queued >= -1);
	assert(current >= 0);

	/* queued song has started: copy queued to current,
	   and notify the clients */

	const int old_current = current;
	current = queued;
	queued = -1;

	if (queue.consume)
		DeleteOrder(pc, old_current);

	idle_add(IDLE_PLAYER);

	SongStarted();
}

const DetachedSong *
playlist::GetQueuedSong() const
{
	return playing && queued >= 0
		? &queue.GetOrder(queued)
		: nullptr;
}

void
playlist::UpdateQueuedSong(PlayerControl &pc, const DetachedSong *prev)
{
	if (!playing)
		return;

	if (prev == nullptr && bulk_edit)
		/* postponed until CommitBulk() to avoid always
		   queueing the first song that is being added (in
		   random mode) */
		return;

	assert(!queue.IsEmpty());
	assert((queued < 0) == (prev == nullptr));

	const int next_order = current >= 0
		? queue.GetNextOrder(current)
		: 0;

	if (next_order == 0 && queue.random && !queue.single) {
		/* shuffle the song order again, so we get a different
		   order each time the playlist is played
		   completely */
		const unsigned current_position =
			queue.OrderToPosition(current);

		queue.ShuffleOrder();

		/* make sure that the current still points to
		   the current song, after the song order has been
		   shuffled */
		current = queue.PositionToOrder(current_position);
	}

	const DetachedSong *const next_song = next_order >= 0
		? &queue.GetOrder(next_order)
		: nullptr;

	if (prev != nullptr && next_song != prev) {
		/* clear the currently queued song */
		pc.LockCancel();
		queued = -1;
	}

	if (next_order >= 0) {
		if (next_song != prev)
			QueueSongOrder(pc, next_order);
		else
			queued = next_order;
	}
}

void
playlist::PlayOrder(PlayerControl &pc, int order)
{
	playing = true;
	queued = -1;

	const DetachedSong &song = queue.GetOrder(order);

	FormatDebug(playlist_domain, "play %i:\"%s\"", order, song.GetURI());

	pc.Play(new DetachedSong(song));
	current = order;

	SongStarted();
}

void
playlist::SyncWithPlayer(PlayerControl &pc)
{
	if (!playing)
		/* this event has reached us out of sync: we aren't
		   playing anymore; ignore the event */
		return;

	pc.Lock();
	const PlayerState pc_state = pc.GetState();
	const DetachedSong *pc_next_song = pc.next_song;
	pc.Unlock();

	if (pc_state == PlayerState::STOP)
		/* the player thread has stopped: check if playback
		   should be restarted with the next song.  That can
		   happen if the playlist isn't filling the queue fast
		   enough */
		ResumePlayback(pc);
	else {
		/* check if the player thread has already started
		   playing the queued song */
		if (pc_next_song == nullptr && queued != -1)
			QueuedSongStarted(pc);

		pc.Lock();
		pc_next_song = pc.next_song;
		pc.Unlock();

		/* make sure the queued song is always set (if
		   possible) */
		if (pc_next_song == nullptr && queued < 0)
			UpdateQueuedSong(pc, nullptr);
	}
}

inline void
playlist::ResumePlayback(PlayerControl &pc)
{
	assert(playing);
	assert(pc.GetState() == PlayerState::STOP);

	const auto error = pc.GetErrorType();
	if (error == PlayerError::NONE)
		error_count = 0;
	else
		++error_count;

	if ((stop_on_error && error != PlayerError::NONE) ||
	    error == PlayerError::OUTPUT ||
	    error_count >= queue.GetLength())
		/* too many errors, or critical error: stop
		   playback */
		Stop(pc);
	else
		/* continue playback at the next song */
		PlayNext(pc);
}

void
playlist::SetRepeat(PlayerControl &pc, bool status)
{
	if (status == queue.repeat)
		return;

	queue.repeat = status;

	pc.LockSetBorderPause(queue.single && !queue.repeat);

	/* if the last song is currently being played, the "next song"
	   might change when repeat mode is toggled */
	UpdateQueuedSong(pc, GetQueuedSong());

	idle_add(IDLE_OPTIONS);
}

static void
playlist_order(playlist &playlist)
{
	if (playlist.current >= 0)
		/* update playlist.current, order==position now */
		playlist.current = playlist.queue.OrderToPosition(playlist.current);

	playlist.queue.RestoreOrder();
}

void
playlist::SetSingle(PlayerControl &pc, bool status)
{
	if (status == queue.single)
		return;

	queue.single = status;

	pc.LockSetBorderPause(queue.single && !queue.repeat);

	/* if the last song is currently being played, the "next song"
	   might change when single mode is toggled */
	UpdateQueuedSong(pc, GetQueuedSong());

	idle_add(IDLE_OPTIONS);
}

void
playlist::SetConsume(bool status)
{
	if (status == queue.consume)
		return;

	queue.consume = status;
	idle_add(IDLE_OPTIONS);
}

void
playlist::SetRandom(PlayerControl &pc, bool status)
{
	if (status == queue.random)
		return;

	const DetachedSong *const queued_song = GetQueuedSong();

	queue.random = status;

	if (queue.random) {
		/* shuffle the queue order, but preserve current */

		const int current_position = playing
			? GetCurrentPosition()
			: -1;

		queue.ShuffleOrder();

		if (current_position >= 0) {
			/* make sure the current song is the first in
			   the order list, so the whole rest of the
			   playlist is played after that */
			unsigned current_order =
				queue.PositionToOrder(current_position);
			queue.SwapOrders(0, current_order);
			current = 0;
		} else
			current = -1;
	} else
		playlist_order(*this);

	UpdateQueuedSong(pc, queued_song);

	idle_add(IDLE_OPTIONS);
}

int
playlist::GetCurrentPosition() const
{
	return current >= 0
		? queue.OrderToPosition(current)
		: -1;
}

int
playlist::GetNextPosition() const
{
	if (current < 0)
		return -1;

	if (queue.single && queue.repeat)
		return queue.OrderToPosition(current);
	else if (queue.IsValidOrder(current + 1))
		return queue.OrderToPosition(current + 1);
	else if (queue.repeat)
		return queue.OrderToPosition(0);

	return -1;
}
