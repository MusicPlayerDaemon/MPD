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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Functions for editing the playlist (adding, removing, reordering
 * songs in the queue).
 *
 */

#include "playlist_internal.h"
#include "player_control.h"
#include "database.h"
#include "uri.h"
#include "song.h"
#include "idle.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

static void incrPlaylistVersion(struct playlist *playlist)
{
	queue_increment_version(&playlist->queue);

	idle_add(IDLE_PLAYLIST);
}

void clearPlaylist(struct playlist *playlist)
{
	stopPlaylist(playlist);

	/* make sure there are no references to allocated songs
	   anymore */
	for (unsigned i = 0; i < queue_length(&playlist->queue); i++) {
		const struct song *song = queue_get(&playlist->queue, i);
		if (!song_in_database(song))
			pc_song_deleted(song);
	}

	queue_clear(&playlist->queue);

	playlist->current = -1;

	incrPlaylistVersion(playlist);
}

#ifndef WIN32
enum playlist_result
playlist_append_file(struct playlist *playlist, const char *path, int uid,
		     unsigned *added_id)
{
	int ret;
	struct stat st;
	struct song *song;

	if (uid <= 0)
		/* unauthenticated client */
		return PLAYLIST_RESULT_DENIED;

	ret = stat(path, &st);
	if (ret < 0)
		return PLAYLIST_RESULT_ERRNO;

	if (st.st_uid != (uid_t)uid && (st.st_mode & 0444) != 0444)
		/* client is not owner */
		return PLAYLIST_RESULT_DENIED;

	song = song_file_load(path, NULL);
	if (song == NULL)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return addSongToPlaylist(playlist, song, added_id);
}
#endif

static struct song *
song_by_url(const char *url)
{
	struct song *song;

	song = db_get_song(url);
	if (song != NULL)
		return song;

	if (uri_has_scheme(url))
		return song_remote_new(url);

	return NULL;
}

enum playlist_result
addToPlaylist(struct playlist *playlist, const char *url, unsigned *added_id)
{
	struct song *song;

	g_debug("add to playlist: %s", url);

	song = song_by_url(url);
	if (song == NULL)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return addSongToPlaylist(playlist, song, added_id);
}

enum playlist_result
addSongToPlaylist(struct playlist *playlist,
		  struct song *song, unsigned *added_id)
{
	const struct song *queued;
	unsigned id;

	if (queue_is_full(&playlist->queue))
		return PLAYLIST_RESULT_TOO_LARGE;

	queued = playlist_get_queued_song(playlist);

	id = queue_append(&playlist->queue, song);

	if (playlist->queue.random) {
		/* shuffle the new song into the list of remaining
		   songs to play */

		unsigned start;
		if (playlist->queued >= 0)
			start = playlist->queued + 1;
		else
			start = playlist->current + 1;
		if (start < queue_length(&playlist->queue))
			queue_shuffle_order_last(&playlist->queue, start,
						 queue_length(&playlist->queue));
	}

	incrPlaylistVersion(playlist);

	playlist_update_queued_song(playlist, queued);

	if (added_id)
		*added_id = id;

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
swapSongsInPlaylist(struct playlist *playlist, unsigned song1, unsigned song2)
{
	const struct song *queued;

	if (!queue_valid_position(&playlist->queue, song1) ||
	    !queue_valid_position(&playlist->queue, song2))
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song(playlist);

	queue_swap(&playlist->queue, song1, song2);

	if (playlist->queue.random) {
		/* update the queue order, so that playlist->current
		   still points to the current song order */

		queue_swap_order(&playlist->queue,
				 queue_position_to_order(&playlist->queue,
							 song1),
				 queue_position_to_order(&playlist->queue,
							 song2));
	} else {
		/* correct the "current" song order */

		if (playlist->current == (int)song1)
			playlist->current = song2;
		else if (playlist->current == (int)song2)
			playlist->current = song1;
	}

	incrPlaylistVersion(playlist);

	playlist_update_queued_song(playlist, queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
swapSongsInPlaylistById(struct playlist *playlist, unsigned id1, unsigned id2)
{
	int song1 = queue_id_to_position(&playlist->queue, id1);
	int song2 = queue_id_to_position(&playlist->queue, id2);

	if (song1 < 0 || song2 < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return swapSongsInPlaylist(playlist, song1, song2);
}

enum playlist_result
deleteFromPlaylist(struct playlist *playlist, unsigned song)
{
	const struct song *queued;
	unsigned songOrder;

	if (song >= queue_length(&playlist->queue))
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song(playlist);

	songOrder = queue_position_to_order(&playlist->queue, song);

	if (playlist->playing && playlist->current == (int)songOrder) {
		bool paused = getPlayerState() == PLAYER_STATE_PAUSE;

		/* the current song is going to be deleted: stop the player */

		playerWait();
		playlist->playing = false;

		/* see which song is going to be played instead */

		playlist->current = queue_next_order(&playlist->queue,
						     playlist->current);
		if (playlist->current == (int)songOrder)
			playlist->current = -1;

		if (playlist->current >= 0 && !paused)
			/* play the song after the deleted one */
			playPlaylistOrderNumber(playlist, playlist->current);
		else
			/* no songs left to play, stop playback
			   completely */
			stopPlaylist(playlist);

		queued = NULL;
	} else if (playlist->current == (int)songOrder)
		/* there's a "current song" but we're not playing
		   currently - clear "current" */
		playlist->current = -1;

	/* now do it: remove the song */

	if (!song_in_database(queue_get(&playlist->queue, song)))
		pc_song_deleted(queue_get(&playlist->queue, song));

	queue_delete(&playlist->queue, song);

	incrPlaylistVersion(playlist);

	/* update the "current" and "queued" variables */

	if (playlist->current > (int)songOrder) {
		playlist->current--;
	}

	playlist_update_queued_song(playlist, queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
deleteFromPlaylistById(struct playlist *playlist, unsigned id)
{
	int song = queue_id_to_position(&playlist->queue, id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return deleteFromPlaylist(playlist, song);
}

void
deleteASongFromPlaylist(struct playlist *playlist, const struct song *song)
{
	for (int i = queue_length(&playlist->queue) - 1; i >= 0; --i)
		if (song == queue_get(&playlist->queue, i))
			deleteFromPlaylist(playlist, i);

	pc_song_deleted(song);
}

enum playlist_result
moveSongInPlaylist(struct playlist *playlist, unsigned from, int to)
{
	const struct song *queued;
	int currentSong;

	if (!queue_valid_position(&playlist->queue, from))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((to >= 0 && to >= (int)queue_length(&playlist->queue)) ||
	    (to < 0 && abs(to) > (int)queue_length(&playlist->queue)))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((int)from == to) /* no-op */
		return PLAYLIST_RESULT_SUCCESS;

	queued = playlist_get_queued_song(playlist);

	/*
	 * (to < 0) => move to offset from current song
	 * (-playlist.length == to) => move to position BEFORE current song
	 */
	currentSong = playlist->current >= 0
		? (int)queue_order_to_position(&playlist->queue,
					      playlist->current)
		: -1;
	if (to < 0 && playlist->current >= 0) {
		if ((unsigned)currentSong == from)
			/* no-op, can't be moved to offset of itself */
			return PLAYLIST_RESULT_SUCCESS;
		to = (currentSong + abs(to)) % queue_length(&playlist->queue);
	}

	queue_move(&playlist->queue, from, to);

	if (!playlist->queue.random) {
		/* update current/queued */
		if (playlist->current == (int)from)
			playlist->current = to;
		else if (playlist->current > (int)from &&
			 playlist->current <= to) {
			playlist->current--;
		} else if (playlist->current >= to &&
			   playlist->current < (int)from) {
			playlist->current++;
		}
	}

	incrPlaylistVersion(playlist);

	playlist_update_queued_song(playlist, queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result
moveSongInPlaylistById(struct playlist *playlist, unsigned id1, int to)
{
	int song = queue_id_to_position(&playlist->queue, id1);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return moveSongInPlaylist(playlist, song, to);
}

void shufflePlaylist(struct playlist *playlist, unsigned start, unsigned end)
{
	const struct song *queued;

	if (end > queue_length(&playlist->queue))
		/* correct the "end" offset */
		end = queue_length(&playlist->queue);

	if ((start+1) >= end)
		/* needs at least two entries. */
		return;

	queued = playlist_get_queued_song(playlist);
	if (playlist->playing && playlist->current >= 0) {
		unsigned current_position;
		current_position = queue_order_to_position(&playlist->queue,
	                                                   playlist->current);

		if (current_position >= start && current_position < end)
		{
			/* put current playing song first */
			queue_swap(&playlist->queue, start, current_position);

			if (playlist->queue.random) {
				playlist->current =
					queue_position_to_order(&playlist->queue, start);
			} else
				playlist->current = start;

			/* start shuffle after the current song */
			start++;
		}
	} else {
		/* no playback currently: reset playlist->current */

		playlist->current = -1;
	}

	queue_shuffle_range(&playlist->queue, start, end);

	incrPlaylistVersion(playlist);

	playlist_update_queued_song(playlist, queued);
}
