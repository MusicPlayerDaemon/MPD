/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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

#include "playlist.h"
#include "playlist_save.h"
#include "playlist_state.h"
#include "queue_print.h"
#include "locate.h"
#include "player_control.h"
#include "command.h"
#include "ls.h"
#include "tag.h"
#include "song.h"
#include "conf.h"
#include "database.h"
#include "mapper.h"
#include "stored_playlist.h"
#include "ack.h"
#include "idle.h"
#include "event_pipe.h"

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

/**
 * When the "prev" command is received, rewind the current track if
 * this number of seconds has already elapsed.
 */
#define PLAYLIST_PREV_UNLESS_ELAPSED    10

/** the global playlist object */
static struct playlist playlist;

static void playPlaylistOrderNumber(int orderNum);

static void incrPlaylistVersion(void)
{
	queue_increment_version(&playlist.queue);

	idle_add(IDLE_PLAYLIST);
}

void playlistVersionChange(void)
{
	queue_modify_all(&playlist.queue);
	idle_add(IDLE_PLAYLIST);
}

static void
playlist_tag_event(void)
{
	if (!playlist.playing)
		return;

	assert(playlist.current >= 0);

	queue_modify(&playlist.queue, playlist.current);
	idle_add(IDLE_PLAYLIST);
}

void initPlaylist(void)
{
	queue_init(&playlist.queue,
		   config_get_positive(CONF_MAX_PLAYLIST_LENGTH,
				       DEFAULT_PLAYLIST_MAX_LENGTH));

	playlist.queued = -1;
	playlist.current = -1;

	event_pipe_register(PIPE_EVENT_TAG, playlist_tag_event);
}

void finishPlaylist(void)
{
	queue_finish(&playlist.queue);
}

const struct queue *
playlist_get_queue(void)
{
	return &playlist.queue;
}

void clearPlaylist(void)
{
	stopPlaylist();

	/* make sure there are no references to allocated songs
	   anymore */
	for (unsigned i = 0; i < queue_length(&playlist.queue); i++) {
		const struct song *song = queue_get(&playlist.queue, i);
		if (!song_in_database(song))
			pc_song_deleted(song);
	}

	queue_clear(&playlist.queue);

	playlist.current = -1;

	incrPlaylistVersion();
}

void savePlaylistState(FILE *fp)
{
	playlist_state_save(fp, &playlist);
}

void readPlaylistState(FILE *fp)
{
	playlist_state_restore(fp, &playlist);
}

/**
 * Queue a song, addressed by its order number.
 */
static void
playlist_queue_song_order(unsigned order)
{
	struct song *song;
	char *uri;

	assert(queue_valid_order(&playlist.queue, order));

	playlist.queued = order;

	song = queue_get_order(&playlist.queue, order);
	uri = song_get_uri(song);
	g_debug("playlist: queue song %i:\"%s\"",
		playlist.queued, uri);
	g_free(uri);

	queueSong(song);
}

/**
 * Check if the player thread has already started playing the "queued"
 * song.
 */
static void syncPlaylistWithQueue(void)
{
	if (pc.next_song == NULL && playlist.queued != -1) {
		/* queued song has started: copy queued to current,
		   and notify the clients */

		playlist.current = playlist.queued;
		playlist.queued = -1;

		idle_add(IDLE_PLAYER);
	}
}

/**
 * Returns the song object which is currently queued.  Returns none if
 * there is none (yet?) or if MPD isn't playing.
 */
static const struct song *
playlist_get_queued_song(void)
{
	if (!playlist.playing || playlist.queued < 0)
		return NULL;

	return queue_get_order(&playlist.queue, playlist.queued);
}

/**
 * Updates the "queued song".  Calculates the next song according to
 * the current one (if MPD isn't playing, it takes the first song),
 * and queues this song.  Clears the old queued song if there was one.
 *
 * @param prev the song which was previously queued, as determined by
 * playlist_get_queued_song()
 */
static void
playlist_update_queued_song(const struct song *prev)
{
	int next_order;
	const struct song *next_song;

	if (!playlist.playing)
		return;

	assert(!queue_is_empty(&playlist.queue));
	assert((playlist.queued < 0) == (prev == NULL));

	next_order = playlist.current >= 0
		? queue_next_order(&playlist.queue, playlist.current)
		: 0;

	if (next_order == 0 && playlist.queue.random) {
		/* shuffle the song order again, so we get a different
		   order each time the playlist is played
		   completely */
		unsigned current_position =
			queue_order_to_position(&playlist.queue,
						playlist.current);

		queue_shuffle_order(&playlist.queue);

		/* make sure that the playlist.current still points to
		   the current song, after the song order has been
		   shuffled */
		playlist.current =
			queue_position_to_order(&playlist.queue,
						current_position);
	}

	if (next_order >= 0)
		next_song = queue_get_order(&playlist.queue, next_order);
	else
		next_song = NULL;

	if (prev != NULL && next_song != prev) {
		/* clear the currently queued song */
		pc_cancel();
		playlist.queued = -1;
	}

	if (next_order >= 0) {
		if (next_song != prev)
			playlist_queue_song_order(next_order);
		else
			playlist.queued = next_order;
	}
}

#ifndef WIN32
enum playlist_result
playlist_append_file(const char *path, int uid, unsigned *added_id)
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

	return addSongToPlaylist(song, added_id);
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

enum playlist_result addToPlaylist(const char *url, unsigned *added_id)
{
	struct song *song;

	g_debug("add to playlist: %s", url);

	song = song_by_url(url);
	if (song == NULL)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return addSongToPlaylist(song, added_id);
}

enum playlist_result
addSongToPlaylist(struct song *song, unsigned *added_id)
{
	const struct song *queued;
	unsigned id;

	if (queue_is_full(&playlist.queue))
		return PLAYLIST_RESULT_TOO_LARGE;

	queued = playlist_get_queued_song();

	id = queue_append(&playlist.queue, song);

	if (playlist.queue.random) {
		/* shuffle the new song into the list of remaining
		   songs to play */

		unsigned start;
		if (playlist.queued >= 0)
			start = playlist.queued + 1;
		else
			start = playlist.current + 1;
		if (start < queue_length(&playlist.queue))
			queue_shuffle_order_last(&playlist.queue, start,
						 queue_length(&playlist.queue));
	}

	incrPlaylistVersion();

	playlist_update_queued_song(queued);

	if (added_id)
		*added_id = id;

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result swapSongsInPlaylist(unsigned song1, unsigned song2)
{
	const struct song *queued;

	if (!queue_valid_position(&playlist.queue, song1) ||
	    !queue_valid_position(&playlist.queue, song2))
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song();

	queue_swap(&playlist.queue, song1, song2);

	if (playlist.queue.random) {
		/* update the queue order, so that playlist.current
		   still points to the current song order */

		queue_swap_order(&playlist.queue,
				 queue_position_to_order(&playlist.queue,
							 song1),
				 queue_position_to_order(&playlist.queue,
							 song2));
	} else {
		/* correct the "current" song order */

		if (playlist.current == (int)song1)
			playlist.current = song2;
		else if (playlist.current == (int)song2)
			playlist.current = song1;
	}

	incrPlaylistVersion();

	playlist_update_queued_song(queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result swapSongsInPlaylistById(unsigned id1, unsigned id2)
{
	int song1 = queue_id_to_position(&playlist.queue, id1);
	int song2 = queue_id_to_position(&playlist.queue, id2);

	if (song1 < 0 || song2 < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return swapSongsInPlaylist(song1, song2);
}

enum playlist_result deleteFromPlaylist(unsigned song)
{
	const struct song *queued;
	unsigned songOrder;

	if (song >= queue_length(&playlist.queue))
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song();

	songOrder = queue_position_to_order(&playlist.queue, song);

	if (playlist.playing && playlist.current == (int)songOrder) {
		bool paused = getPlayerState() == PLAYER_STATE_PAUSE;

		/* the current song is going to be deleted: stop the player */

		playerWait();
		playlist.playing = false;

		/* see which song is going to be played instead */

		playlist.current = queue_next_order(&playlist.queue,
						    playlist.current);
		if (playlist.current == (int)songOrder)
			playlist.current = -1;

		if (playlist.current >= 0 && !paused)
			/* play the song after the deleted one */
			playPlaylistOrderNumber(playlist.current);
		else
			/* no songs left to play, stop playback
			   completely */
			stopPlaylist();

		queued = NULL;
	}

	/* now do it: remove the song */

	if (!song_in_database(queue_get(&playlist.queue, song)))
		pc_song_deleted(queue_get(&playlist.queue, song));

	queue_delete(&playlist.queue, song);

	incrPlaylistVersion();

	/* update the "current" and "queued" variables */

	if (playlist.current > (int)songOrder) {
		playlist.current--;
	}

	playlist_update_queued_song(queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result deleteFromPlaylistById(unsigned id)
{
	int song = queue_id_to_position(&playlist.queue, id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return deleteFromPlaylist(song);
}

void
deleteASongFromPlaylist(const struct song *song)
{
	for (int i = queue_length(&playlist.queue) - 1; i >= 0; --i)
		if (song == queue_get(&playlist.queue, i))
			deleteFromPlaylist(i);

	pc_song_deleted(song);
}

void stopPlaylist(void)
{
	if (!playlist.playing)
		return;

	assert(playlist.current >= 0);

	g_debug("playlist: stop");
	playerWait();
	playlist.queued = -1;
	playlist.playing = false;

	if (playlist.queue.random) {
		/* shuffle the playlist, so the next playback will
		   result in a new random order */

		unsigned current_position =
			queue_order_to_position(&playlist.queue,
						playlist.current);

		queue_shuffle_order(&playlist.queue);

		/* make sure that "current" stays valid, and the next
		   "play" command plays the same song again */
		playlist.current =
			queue_position_to_order(&playlist.queue,
						current_position);
	}
}

static void playPlaylistOrderNumber(int orderNum)
{
	struct song *song;
	char *uri;

	playlist.playing = true;
	playlist.queued = -1;

	song = queue_get_order(&playlist.queue, orderNum);

	uri = song_get_uri(song);
	g_debug("playlist: play %i:\"%s\"", orderNum, uri);
	g_free(uri);

	playerPlay(song);
	playlist.current = orderNum;
}

enum playlist_result playPlaylist(int song)
{
	unsigned i = song;

	clearPlayerError();

	if (song == -1) {
		/* play any song ("current" song, or the first song */

		if (queue_is_empty(&playlist.queue))
			return PLAYLIST_RESULT_SUCCESS;

		if (playlist.playing) {
			/* already playing: unpause playback, just in
			   case it was paused, and return */
			playerSetPause(0);
			return PLAYLIST_RESULT_SUCCESS;
		}

		/* select a song: "current" song, or the first one */
		i = playlist.current >= 0
			? playlist.current
			: 0;
	} else if (!queue_valid_position(&playlist.queue, song))
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist.queue.random) {
		if (song >= 0)
			/* "i" is currently the song position (which
			   would be equal to the order number in
			   no-random mode); convert it to a order
			   number, because random mode is enabled */
			i = queue_position_to_order(&playlist.queue, song);

		if (!playlist.playing)
			playlist.current = 0;

		/* swap the new song with the previous "current" one,
		   so playback continues as planned */
		queue_swap_order(&playlist.queue,
				 i, playlist.current);
		i = playlist.current;
	}

	playlist.stop_on_error = false;
	playlist.error_count = 0;

	playPlaylistOrderNumber(i);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result playPlaylistById(int id)
{
	int song;

	if (id == -1) {
		return playPlaylist(id);
	}

	song = queue_id_to_position(&playlist.queue, id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playPlaylist(song);
}

static void playPlaylistIfPlayerStopped(void);

/**
 * This is the "PLAYLIST" event handler.  It is invoked by the player
 * thread whenever it requests a new queued song, or when it exits.
 */
void syncPlayerAndPlaylist(void)
{
	if (!playlist.playing)
		/* this event has reached us out of sync: we aren't
		   playing anymore; ignore the event */
		return;

	if (getPlayerState() == PLAYER_STATE_STOP)
		/* the player thread has stopped: check if playback
		   should be restarted with the next song.  That can
		   happen if the playlist isn't filling the queue fast
		   enough */
		playPlaylistIfPlayerStopped();
	else {
		/* check if the player thread has already started
		   playing the queued song */
		syncPlaylistWithQueue();

		/* make sure the queued song is always set (if
		   possible) */
		if (pc.next_song == NULL)
			playlist_update_queued_song(NULL);
	}
}

void nextSongInPlaylist(void)
{
	int next_order;

	if (!playlist.playing)
		return;

	assert(!queue_is_empty(&playlist.queue));
	assert(queue_valid_order(&playlist.queue, playlist.current));

	syncPlaylistWithQueue();

	playlist.stop_on_error = false;

	/* determine the next song from the queue's order list */

	next_order = queue_next_order(&playlist.queue, playlist.current);
	if (next_order < 0) {
		/* no song after this one: stop playback */
		stopPlaylist();
		return;
	}

	if (next_order == 0 && playlist.queue.random) {
		/* The queue told us that the next song is the first
		   song.  This means we are in repeat mode.  Shuffle
		   the queue order, so this time, the user hears the
		   songs in a different than before */
		assert(playlist.queue.repeat);

		queue_shuffle_order(&playlist.queue);

		/* note that playlist.current and playlist.queued are
		   now invalid, but playPlaylistOrderNumber() will
		   discard them anyway */
	}

	playPlaylistOrderNumber(next_order);
}

/**
 * The player has stopped for some reason.  Check the error, and
 * decide whether to re-start playback
 */
static void playPlaylistIfPlayerStopped(void)
{
	enum player_error error;

	assert(playlist.playing);
	assert(getPlayerState() == PLAYER_STATE_STOP);

	error = getPlayerError();
	if (error == PLAYER_ERROR_NOERROR)
		playlist.error_count = 0;
	else
		++playlist.error_count;

	if ((playlist.stop_on_error && error != PLAYER_ERROR_NOERROR) ||
	    error == PLAYER_ERROR_AUDIO || error == PLAYER_ERROR_SYSTEM ||
	    playlist.error_count >= queue_length(&playlist.queue))
		/* too many errors, or critical error: stop
		   playback */
		stopPlaylist();
	else
		/* continue playback at the next song */
		nextSongInPlaylist();
}

bool getPlaylistRepeatStatus(void)
{
	return playlist.queue.repeat;
}

bool getPlaylistRandomStatus(void)
{
	return playlist.queue.random;
}

void setPlaylistRepeatStatus(bool status)
{
	if (status == playlist.queue.repeat)
		return;

	playlist.queue.repeat = status;

	/* if the last song is currently being played, the "next song"
	   might change when repeat mode is toggled */
	playlist_update_queued_song(playlist_get_queued_song());

	idle_add(IDLE_OPTIONS);
}

enum playlist_result moveSongInPlaylist(unsigned from, int to)
{
	const struct song *queued;
	int currentSong;

	if (!queue_valid_position(&playlist.queue, from))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((to >= 0 && to >= (int)queue_length(&playlist.queue)) ||
	    (to < 0 && abs(to) > (int)queue_length(&playlist.queue)))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((int)from == to) /* no-op */
		return PLAYLIST_RESULT_SUCCESS;

	queued = playlist_get_queued_song();

	/*
	 * (to < 0) => move to offset from current song
	 * (-playlist.length == to) => move to position BEFORE current song
	 */
	currentSong = playlist.current >= 0
		? (int)queue_order_to_position(&playlist.queue,
					      playlist.current)
		: -1;
	if (to < 0 && playlist.current >= 0) {
		if ((unsigned)currentSong == from)
			/* no-op, can't be moved to offset of itself */
			return PLAYLIST_RESULT_SUCCESS;
		to = (currentSong + abs(to)) % queue_length(&playlist.queue);
	}

	queue_move(&playlist.queue, from, to);

	if (!playlist.queue.random) {
		/* update current/queued */
		if (playlist.current == (int)from)
			playlist.current = to;
		else if (playlist.current > (int)from &&
			 playlist.current <= to) {
			playlist.current--;
		} else if (playlist.current >= to &&
			   playlist.current < (int)from) {
			playlist.current++;
		}
	}

	incrPlaylistVersion();

	playlist_update_queued_song(queued);

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result moveSongInPlaylistById(unsigned id1, int to)
{
	int song = queue_id_to_position(&playlist.queue, id1);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return moveSongInPlaylist(song, to);
}

static void orderPlaylist(void)
{
	if (playlist.current >= 0)
		/* update playlist.current, order==position now */
		playlist.current = queue_order_to_position(&playlist.queue,
							   playlist.current);

	queue_restore_order(&playlist.queue);
}

void setPlaylistRandomStatus(bool status)
{
	const struct song *queued;

	if (status == playlist.queue.random)
		return;

	queued = playlist_get_queued_song();

	playlist.queue.random = status;

	if (playlist.queue.random) {
		/* shuffle the queue order, but preserve
		   playlist.current */

		int current_position = playlist.current >= 0
			? (int)queue_order_to_position(&playlist.queue,
						       playlist.current)
			: -1;

		queue_shuffle_order(&playlist.queue);

		if (current_position >= 0) {
			/* make sure the current song is the first in
			   the order list, so the whole rest of the
			   playlist is played after that */
			unsigned current_order =
				queue_position_to_order(&playlist.queue,
							current_position);
			queue_swap_order(&playlist.queue, 0, current_order);
			playlist.current = 0;
		}
	} else
		orderPlaylist();

	playlist_update_queued_song(queued);

	idle_add(IDLE_OPTIONS);
}

void previousSongInPlaylist(void)
{
	static time_t lastTime;
	time_t diff = time(NULL) - lastTime;

	lastTime += diff;

	if (!playlist.playing)
		return;

	syncPlaylistWithQueue();

	if (diff && getPlayerElapsedTime() > PLAYLIST_PREV_UNLESS_ELAPSED) {
		/* re-start playing the current song (just like the
		   "prev" button on CD players) */

		playPlaylistOrderNumber(playlist.current);
	} else {
		if (playlist.current > 0) {
			/* play the preceding song */
			playPlaylistOrderNumber(playlist.current - 1);
		} else if (playlist.queue.repeat) {
			/* play the last song in "repeat" mode */
			playPlaylistOrderNumber(queue_length(&playlist.queue) - 1);
		} else {
			/* re-start playing the current song if it's
			   the first one */
			playPlaylistOrderNumber(playlist.current);
		}
	}
}

void shufflePlaylist(void)
{
	const struct song *queued;
	unsigned i;

	if (queue_length(&playlist.queue) <= 1)
		return;

	queued = playlist_get_queued_song();

	if (playlist.playing) {
		if (playlist.current >= 0)
			/* put current playing song first */
			queue_swap(&playlist.queue, 0,
				   queue_order_to_position(&playlist.queue,
							   playlist.current));

		if (playlist.queue.random) {
			playlist.current =
				queue_position_to_order(&playlist.queue, 0);
		} else
			playlist.current = 0;

		/* start shuffle after the current song */
		i = 1;
	} else {
		/* no playback currently: shuffle everything, and
		   reset playlist.current */

		i = 0;
		playlist.current = -1;
	}

	/* shuffle the rest of the list */
	queue_shuffle_range(&playlist.queue, i,
			    queue_length(&playlist.queue));

	incrPlaylistVersion();

	playlist_update_queued_song(queued);
}

int getPlaylistCurrentSong(void)
{
	if (playlist.current >= 0)
		return queue_order_to_position(&playlist.queue,
					       playlist.current);

	return -1;
}

unsigned long getPlaylistVersion(void)
{
	return playlist.queue.version;
}

int getPlaylistLength(void)
{
	return queue_length(&playlist.queue);
}

enum playlist_result seekSongInPlaylist(unsigned song, float seek_time)
{
	const struct song *queued;
	unsigned i;
	int ret;

	if (!queue_valid_position(&playlist.queue, song))
		return PLAYLIST_RESULT_BAD_RANGE;

	queued = playlist_get_queued_song();

	if (playlist.queue.random)
		i = queue_position_to_order(&playlist.queue, song);
	else
		i = song;

	clearPlayerError();
	playlist.stop_on_error = true;
	playlist.error_count = 0;

	if (!playlist.playing || (unsigned)playlist.current != i) {
		/* seeking is not within the current song - first
		   start playing the new song */

		playPlaylistOrderNumber(i);
		queued = NULL;
	}

	playlist_update_queued_song(queued);

	ret = playerSeek(queue_get_order(&playlist.queue, i), seek_time);
	if (ret < 0)
		return PLAYLIST_RESULT_NOT_PLAYING;

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result seekSongInPlaylistById(unsigned id, float seek_time)
{
	int song = queue_id_to_position(&playlist.queue, id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return seekSongInPlaylist(song, seek_time);
}

unsigned getPlaylistSongId(unsigned song)
{
	return queue_position_to_id(&playlist.queue, song);
}
