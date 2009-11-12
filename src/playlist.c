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

#include "config.h"
#include "playlist_internal.h"
#include "playlist_save.h"
#include "player_control.h"
#include "command.h"
#include "tag.h"
#include "song.h"
#include "conf.h"
#include "stored_playlist.h"
#include "idle.h"

#include <glib.h>

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "playlist"

void
playlist_increment_version_all(struct playlist *playlist)
{
	queue_modify_all(&playlist->queue);
	idle_add(IDLE_PLAYLIST);
}

void
playlist_tag_changed(struct playlist *playlist)
{
	if (!playlist->playing)
		return;

	assert(playlist->current >= 0);

	queue_modify(&playlist->queue, playlist->current);
	idle_add(IDLE_PLAYLIST);
}

void
playlist_init(struct playlist *playlist)
{
	queue_init(&playlist->queue,
		   config_get_positive(CONF_MAX_PLAYLIST_LENGTH,
				       DEFAULT_PLAYLIST_MAX_LENGTH));

	playlist->queued = -1;
	playlist->current = -1;
}

void
playlist_finish(struct playlist *playlist)
{
	queue_finish(&playlist->queue);
}

/**
 * Queue a song, addressed by its order number.
 */
static void
playlist_queue_song_order(struct playlist *playlist, unsigned order)
{
	struct song *song;
	char *uri;

	assert(queue_valid_order(&playlist->queue, order));

	playlist->queued = order;

	song = queue_get_order(&playlist->queue, order);
	uri = song_get_uri(song);
	g_debug("queue song %i:\"%s\"", playlist->queued, uri);
	g_free(uri);

	pc_enqueue_song(song);
}

/**
 * Check if the player thread has already started playing the "queued"
 * song.
 */
static void
playlist_sync_with_queue(struct playlist *playlist)
{
	if (pc.next_song == NULL && playlist->queued != -1) {
		/* queued song has started: copy queued to current,
		   and notify the clients */

		int current = playlist->current;
		playlist->current = playlist->queued;
		playlist->queued = -1;

		if(playlist->queue.consume)
			playlist_delete(playlist, queue_order_to_position(&playlist->queue, current));

		idle_add(IDLE_PLAYER);
	}
}

const struct song *
playlist_get_queued_song(struct playlist *playlist)
{
	if (!playlist->playing || playlist->queued < 0)
		return NULL;

	return queue_get_order(&playlist->queue, playlist->queued);
}

void
playlist_update_queued_song(struct playlist *playlist, const struct song *prev)
{
	int next_order;
	const struct song *next_song;

	if (!playlist->playing)
		return;

	assert(!queue_is_empty(&playlist->queue));
	assert((playlist->queued < 0) == (prev == NULL));

	next_order = playlist->current >= 0
		? queue_next_order(&playlist->queue, playlist->current)
		: 0;

	if (next_order == 0 && playlist->queue.random) {
		/* shuffle the song order again, so we get a different
		   order each time the playlist is played
		   completely */
		unsigned current_position =
			queue_order_to_position(&playlist->queue,
						playlist->current);

		queue_shuffle_order(&playlist->queue);

		/* make sure that the playlist->current still points to
		   the current song, after the song order has been
		   shuffled */
		playlist->current =
			queue_position_to_order(&playlist->queue,
						current_position);
	}

	if (next_order >= 0)
		next_song = queue_get_order(&playlist->queue, next_order);
	else
		next_song = NULL;

	if (prev != NULL && next_song != prev) {
		/* clear the currently queued song */
		pc_cancel();
		playlist->queued = -1;
	}

	if (next_order >= 0) {
		if (next_song != prev)
			playlist_queue_song_order(playlist, next_order);
		else
			playlist->queued = next_order;
	}
}

void
playlist_play_order(struct playlist *playlist, int orderNum)
{
	struct song *song;
	char *uri;

	playlist->playing = true;
	playlist->queued = -1;

	song = queue_get_order(&playlist->queue, orderNum);

	uri = song_get_uri(song);
	g_debug("play %i:\"%s\"", orderNum, uri);
	g_free(uri);

	pc_play(song);
	playlist->current = orderNum;
}

static void
playlist_resume_playback(struct playlist *playlist);

/**
 * This is the "PLAYLIST" event handler.  It is invoked by the player
 * thread whenever it requests a new queued song, or when it exits.
 */
void
playlist_sync(struct playlist *playlist)
{
	if (!playlist->playing)
		/* this event has reached us out of sync: we aren't
		   playing anymore; ignore the event */
		return;

	if (pc_get_state() == PLAYER_STATE_STOP)
		/* the player thread has stopped: check if playback
		   should be restarted with the next song.  That can
		   happen if the playlist isn't filling the queue fast
		   enough */
		playlist_resume_playback(playlist);
	else {
		/* check if the player thread has already started
		   playing the queued song */
		playlist_sync_with_queue(playlist);

		/* make sure the queued song is always set (if
		   possible) */
		if (pc.next_song == NULL)
			playlist_update_queued_song(playlist, NULL);
	}
}

/**
 * The player has stopped for some reason.  Check the error, and
 * decide whether to re-start playback
 */
static void
playlist_resume_playback(struct playlist *playlist)
{
	enum player_error error;

	assert(playlist->playing);
	assert(pc_get_state() == PLAYER_STATE_STOP);

	error = pc_get_error();
	if (error == PLAYER_ERROR_NOERROR)
		playlist->error_count = 0;
	else
		++playlist->error_count;

	if ((playlist->stop_on_error && error != PLAYER_ERROR_NOERROR) ||
	    error == PLAYER_ERROR_AUDIO || error == PLAYER_ERROR_SYSTEM ||
	    playlist->error_count >= queue_length(&playlist->queue))
		/* too many errors, or critical error: stop
		   playback */
		playlist_stop(playlist);
	else
		/* continue playback at the next song */
		playlist_next(playlist);
}

bool
playlist_get_repeat(const struct playlist *playlist)
{
	return playlist->queue.repeat;
}

bool
playlist_get_random(const struct playlist *playlist)
{
	return playlist->queue.random;
}

bool
playlist_get_single(const struct playlist *playlist)
{
	return playlist->queue.single;
}

bool
playlist_get_consume(const struct playlist *playlist)
{
	return playlist->queue.consume;
}

void
playlist_set_repeat(struct playlist *playlist, bool status)
{
	if (status == playlist->queue.repeat)
		return;

	playlist->queue.repeat = status;

	/* if the last song is currently being played, the "next song"
	   might change when repeat mode is toggled */
	playlist_update_queued_song(playlist,
				    playlist_get_queued_song(playlist));

	idle_add(IDLE_OPTIONS);
}

static void
playlist_order(struct playlist *playlist)
{
	if (playlist->current >= 0)
		/* update playlist.current, order==position now */
		playlist->current = queue_order_to_position(&playlist->queue,
							    playlist->current);

	queue_restore_order(&playlist->queue);
}

void
playlist_set_single(struct playlist *playlist, bool status)
{
	if (status == playlist->queue.single)
		return;

	playlist->queue.single = status;

	/* if the last song is currently being played, the "next song"
	   might change when single mode is toggled */
	playlist_update_queued_song(playlist,
				    playlist_get_queued_song(playlist));

	idle_add(IDLE_OPTIONS);
}

void
playlist_set_consume(struct playlist *playlist, bool status)
{
	if (status == playlist->queue.consume)
		return;

	playlist->queue.consume = status;
	idle_add(IDLE_OPTIONS);
}

void
playlist_set_random(struct playlist *playlist, bool status)
{
	const struct song *queued;

	if (status == playlist->queue.random)
		return;

	queued = playlist_get_queued_song(playlist);

	playlist->queue.random = status;

	if (playlist->queue.random) {
		/* shuffle the queue order, but preserve
		   playlist->current */

		int current_position =
			playlist->playing && playlist->current >= 0
			? (int)queue_order_to_position(&playlist->queue,
						       playlist->current)
			: -1;

		queue_shuffle_order(&playlist->queue);

		if (current_position >= 0) {
			/* make sure the current song is the first in
			   the order list, so the whole rest of the
			   playlist is played after that */
			unsigned current_order =
				queue_position_to_order(&playlist->queue,
							current_position);
			queue_swap_order(&playlist->queue, 0, current_order);
			playlist->current = 0;
		} else
			playlist->current = -1;
	} else
		playlist_order(playlist);

	playlist_update_queued_song(playlist, queued);

	idle_add(IDLE_OPTIONS);
}

int
playlist_get_current_song(const struct playlist *playlist)
{
	if (playlist->current >= 0)
		return queue_order_to_position(&playlist->queue,
					       playlist->current);

	return -1;
}

int
playlist_get_next_song(const struct playlist *playlist)
{
	if (playlist->current >= 0)
	{
		if (playlist->queue.single == 1)
		{
			if (playlist->queue.repeat == 1)
				return queue_order_to_position(&playlist->queue,
			                              playlist->current);
			else
				return -1;
		}
		if (playlist->current + 1 < (int)queue_length(&playlist->queue))
			return queue_order_to_position(&playlist->queue,
					       playlist->current + 1);
		else if (playlist->queue.repeat == 1)
			return queue_order_to_position(&playlist->queue, 0);
	}

	return -1;
}

unsigned long
playlist_get_version(const struct playlist *playlist)
{
	return playlist->queue.version;
}

int
playlist_get_length(const struct playlist *playlist)
{
	return queue_length(&playlist->queue);
}

unsigned
playlist_get_song_id(const struct playlist *playlist, unsigned song)
{
	return queue_position_to_id(&playlist->queue, song);
}
