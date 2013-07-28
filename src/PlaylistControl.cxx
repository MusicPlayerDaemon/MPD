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
#include "Playlist.hxx"
#include "PlayerControl.hxx"
#include "Song.hxx"

#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "playlist"

void
playlist::Stop(player_control &pc)
{
	if (!playing)
		return;

	assert(current >= 0);

	g_debug("stop");
	pc.Stop();
	queued = -1;
	playing = false;

	if (queue.random) {
		/* shuffle the playlist, so the next playback will
		   result in a new random order */

		unsigned current_position = queue.OrderToPosition(current);

		queue.ShuffleOrder();

		/* make sure that "current" stays valid, and the next
		   "play" command plays the same song again */
		current = queue.PositionToOrder(current_position);
	}
}

enum playlist_result
playlist::PlayPosition(player_control &pc, int song)
{
	pc.ClearError();

	unsigned i = song;
	if (song == -1) {
		/* play any song ("current" song, or the first song */

		if (queue.IsEmpty())
			return PLAYLIST_RESULT_SUCCESS;

		if (playing) {
			/* already playing: unpause playback, just in
			   case it was paused, and return */
			pc.SetPause(false);
			return PLAYLIST_RESULT_SUCCESS;
		}

		/* select a song: "current" song, or the first one */
		i = current >= 0
			? current
			: 0;
	} else if (!queue.IsValidPosition(song))
		return PLAYLIST_RESULT_BAD_RANGE;

	if (queue.random) {
		if (song >= 0)
			/* "i" is currently the song position (which
			   would be equal to the order number in
			   no-random mode); convert it to a order
			   number, because random mode is enabled */
			i = queue.PositionToOrder(song);

		if (!playing)
			current = 0;

		/* swap the new song with the previous "current" one,
		   so playback continues as planned */
		queue.SwapOrders(i, current);
		i = current;
	}

	stop_on_error = false;
	error_count = 0;

	PlayOrder(pc, i);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist::PlayId(player_control &pc, int id)
{
	if (id == -1)
		return PlayPosition(pc, id);

	int song = queue.IdToPosition(id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return PlayPosition(pc, song);
}

void
playlist::PlayNext(player_control &pc)
{
	if (!playing)
		return;

	assert(!queue.IsEmpty());
	assert(queue.IsValidOrder(current));

	const int old_current = current;
	stop_on_error = false;

	/* determine the next song from the queue's order list */

	const int next_order = queue.GetNextOrder(current);
	if (next_order < 0) {
		/* no song after this one: stop playback */
		Stop(pc);

		/* reset "current song" */
		current = -1;
	}
	else
	{
		if (next_order == 0 && queue.random) {
			/* The queue told us that the next song is the first
			   song.  This means we are in repeat mode.  Shuffle
			   the queue order, so this time, the user hears the
			   songs in a different than before */
			assert(queue.repeat);

			queue.ShuffleOrder();

			/* note that current and queued are
			   now invalid, but playlist_play_order() will
			   discard them anyway */
		}

		PlayOrder(pc, next_order);
	}

	/* Consume mode removes each played songs. */
	if (queue.consume)
		DeleteOrder(pc, old_current);
}

void
playlist::PlayPrevious(player_control &pc)
{
	if (!playing)
		return;

	assert(!queue.IsEmpty());

	int order;
	if (current > 0) {
		/* play the preceding song */
		order = current - 1;
	} else if (queue.repeat) {
		/* play the last song in "repeat" mode */
		order = queue.GetLength() - 1;
	} else {
		/* re-start playing the current song if it's
		   the first one */
		order = current;
	}

	PlayOrder(pc, order);
}

enum playlist_result
playlist::SeekSongPosition(player_control &pc, unsigned song, float seek_time)
{
	if (!queue.IsValidPosition(song))
		return PLAYLIST_RESULT_BAD_RANGE;

	const Song *queued_song = GetQueuedSong();

	unsigned i = queue.random
		? queue.PositionToOrder(song)
		: song;

	pc.ClearError();
	stop_on_error = true;
	error_count = 0;

	if (!playing || (unsigned)current != i) {
		/* seeking is not within the current song - prepare
		   song change */

		playing = true;
		current = i;

		queued_song = nullptr;
	}

	Song *the_song = queue.GetOrder(i)->DupDetached();
	if (!pc.Seek(the_song, seek_time)) {
		UpdateQueuedSong(pc, queued_song);

		return PLAYLIST_RESULT_NOT_PLAYING;
	}

	queued = -1;
	UpdateQueuedSong(pc, NULL);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
playlist::SeekSongId(player_control &pc, unsigned id, float seek_time)
{
	int song = queue.IdToPosition(id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return SeekSongPosition(pc, song, seek_time);
}

enum playlist_result
playlist::SeekCurrent(player_control &pc, float seek_time, bool relative)
{
	if (!playing)
		return PLAYLIST_RESULT_NOT_PLAYING;

	if (relative) {
		const auto status = pc.GetStatus();

		if (status.state != PLAYER_STATE_PLAY &&
		    status.state != PLAYER_STATE_PAUSE)
			return PLAYLIST_RESULT_NOT_PLAYING;

		seek_time += (int)status.elapsed_time;
	}

	if (seek_time < 0)
		seek_time = 0;

	return SeekSongPosition(pc, current, seek_time);
}
