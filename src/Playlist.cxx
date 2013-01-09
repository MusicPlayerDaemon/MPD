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

#include "config.h"
#include "Playlist.hxx"
#include "PlayerControl.hxx"
#include "song.h"
#include "Idle.hxx"

#include <glib.h>

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "playlist"

void
playlist_increment_version_all(struct playlist *playlist)
{
	playlist->queue.ModifyAll();
	idle_add(IDLE_PLAYLIST);
}

void
playlist::TagChanged()
{
	if (!playing)
		return;

	assert(current >= 0);

	queue.ModifyAtOrder(current);
	idle_add(IDLE_PLAYLIST);
}

/**
 * Queue a song, addressed by its order number.
 */
static void
playlist_queue_song_order(struct playlist *playlist, struct player_control *pc,
			  unsigned order)
{
	char *uri;

	assert(playlist->queue.IsValidOrder(order));

	playlist->queued = order;

	struct song *song =
		song_dup_detached(playlist->queue.GetOrder(order));

	uri = song_get_uri(song);
	g_debug("queue song %i:\"%s\"", playlist->queued, uri);
	g_free(uri);

	pc_enqueue_song(pc, song);
}

/**
 * Called if the player thread has started playing the "queued" song.
 */
static void
playlist_song_started(struct playlist *playlist, struct player_control *pc)
{
	assert(pc->next_song == NULL);
	assert(playlist->queued >= -1);

	/* queued song has started: copy queued to current,
	   and notify the clients */

	int current = playlist->current;
	playlist->current = playlist->queued;
	playlist->queued = -1;

	if(playlist->queue.consume)
		playlist->DeleteOrder(*pc, current);

	idle_add(IDLE_PLAYER);
}

const struct song *
playlist::GetQueuedSong() const
{
	return playing && queued >= 0
		? queue.GetOrder(queued)
		: nullptr;
}

void
playlist::UpdateQueuedSong(player_control &pc, const song *prev)
{
	if (!playing)
		return;

	assert(!queue.IsEmpty());
	assert((queued < 0) == (prev == NULL));

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

	const struct song *const next_song = next_order >= 0
		? queue.GetOrder(next_order)
		: nullptr;

	if (prev != NULL && next_song != prev) {
		/* clear the currently queued song */
		pc_cancel(&pc);
		queued = -1;
	}

	if (next_order >= 0) {
		if (next_song != prev)
			playlist_queue_song_order(this, &pc, next_order);
		else
			queued = next_order;
	}
}

void
playlist::PlayOrder(player_control &pc, int order)
{
	playing = true;
	queued = -1;

	struct song *song = song_dup_detached(queue.GetOrder(order));

	char *uri = song_get_uri(song);
	g_debug("play %i:\"%s\"", order, uri);
	g_free(uri);

	pc_play(&pc, song);
	current = order;
}

static void
playlist_resume_playback(struct playlist *playlist, struct player_control *pc);

void
playlist::SyncWithPlayer(player_control &pc)
{
	if (!playing)
		/* this event has reached us out of sync: we aren't
		   playing anymore; ignore the event */
		return;

	player_lock(&pc);
	const enum player_state pc_state = pc_get_state(&pc);
	const song *pc_next_song = pc.next_song;
	player_unlock(&pc);

	if (pc_state == PLAYER_STATE_STOP)
		/* the player thread has stopped: check if playback
		   should be restarted with the next song.  That can
		   happen if the playlist isn't filling the queue fast
		   enough */
		playlist_resume_playback(this, &pc);
	else {
		/* check if the player thread has already started
		   playing the queued song */
		if (pc_next_song == nullptr && queued != -1)
			playlist_song_started(this, &pc);

		player_lock(&pc);
		pc_next_song = pc.next_song;
		player_unlock(&pc);

		/* make sure the queued song is always set (if
		   possible) */
		if (pc_next_song == nullptr && queued < 0)
			UpdateQueuedSong(pc, nullptr);
	}
}

/**
 * The player has stopped for some reason.  Check the error, and
 * decide whether to re-start playback
 */
static void
playlist_resume_playback(struct playlist *playlist, struct player_control *pc)
{
	enum player_error error;

	assert(playlist->playing);
	assert(pc_get_state(pc) == PLAYER_STATE_STOP);

	error = pc_get_error_type(pc);
	if (error == PLAYER_ERROR_NONE)
		playlist->error_count = 0;
	else
		++playlist->error_count;

	if ((playlist->stop_on_error && error != PLAYER_ERROR_NONE) ||
	    error == PLAYER_ERROR_OUTPUT ||
	    playlist->error_count >= playlist->queue.GetLength())
		/* too many errors, or critical error: stop
		   playback */
		playlist->Stop(*pc);
	else
		/* continue playback at the next song */
		playlist->PlayNext(*pc);
}

void
playlist::SetRepeat(player_control &pc, bool status)
{
	if (status == queue.repeat)
		return;

	queue.repeat = status;

	pc_set_border_pause(&pc, queue.single && !queue.repeat);

	/* if the last song is currently being played, the "next song"
	   might change when repeat mode is toggled */
	UpdateQueuedSong(pc, GetQueuedSong());

	idle_add(IDLE_OPTIONS);
}

static void
playlist_order(struct playlist *playlist)
{
	if (playlist->current >= 0)
		/* update playlist.current, order==position now */
		playlist->current = playlist->queue.OrderToPosition(playlist->current);

	playlist->queue.RestoreOrder();
}

void
playlist::SetSingle(player_control &pc, bool status)
{
	if (status == queue.single)
		return;

	queue.single = status;

	pc_set_border_pause(&pc, queue.single && !queue.repeat);

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
playlist::SetRandom(player_control &pc, bool status)
{
	if (status == queue.random)
		return;

	const struct song *const queued_song = GetQueuedSong();

	queue.random = status;

	if (queue.random) {
		/* shuffle the queue order, but preserve current */

		const int current_position = GetCurrentPosition();

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
		playlist_order(this);

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
