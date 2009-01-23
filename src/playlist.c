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
#include "player_control.h"
#include "command.h"
#include "ls.h"
#include "tag.h"
#include "song.h"
#include "song_print.h"
#include "client.h"
#include "conf.h"
#include "database.h"
#include "mapper.h"
#include "path.h"
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

#define PLAYLIST_PREV_UNLESS_ELAPSED    10

#define PLAYLIST_STATE_FILE_STATE		"state: "
#define PLAYLIST_STATE_FILE_RANDOM		"random: "
#define PLAYLIST_STATE_FILE_REPEAT		"repeat: "
#define PLAYLIST_STATE_FILE_CURRENT		"current: "
#define PLAYLIST_STATE_FILE_TIME		"time: "
#define PLAYLIST_STATE_FILE_CROSSFADE		"crossfade: "
#define PLAYLIST_STATE_FILE_PLAYLIST_BEGIN	"playlist_begin"
#define PLAYLIST_STATE_FILE_PLAYLIST_END	"playlist_end"

#define PLAYLIST_STATE_FILE_STATE_PLAY		"play"
#define PLAYLIST_STATE_FILE_STATE_PAUSE		"pause"
#define PLAYLIST_STATE_FILE_STATE_STOP		"stop"

#define PLAYLIST_BUFFER_SIZE	2*MPD_PATH_MAX

#define PLAYLIST_HASH_MULT	4

#define DEFAULT_PLAYLIST_MAX_LENGTH		(1024*16)
#define DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS	false

static GRand *g_rand;
static Playlist playlist;
unsigned playlist_max_length;
static int playlist_stopOnError;
static unsigned playlist_errorCount;

bool playlist_saveAbsolutePaths = DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS;

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
	g_rand = g_rand_new();

	playlist_max_length = config_get_positive(CONF_MAX_PLAYLIST_LENGTH,
						  DEFAULT_PLAYLIST_MAX_LENGTH);

	queue_init(&playlist.queue, playlist_max_length);

	playlist.queued = -1;
	playlist.current = -1;

	playlist_saveAbsolutePaths =
		config_get_bool(CONF_SAVE_ABSOLUTE_PATHS,
				DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS);

	event_pipe_register(PIPE_EVENT_TAG, playlist_tag_event);
}

void finishPlaylist(void)
{
	queue_finish(&playlist.queue);

	g_rand_free(g_rand);
	g_rand = NULL;
}

void clearPlaylist(void)
{
	stopPlaylist();

	for (unsigned i = 0; i < queue_length(&playlist.queue); i++) {
		const struct song *song = queue_get(&playlist.queue, i);
		if (!song_in_database(song))
			pc_song_deleted(song);
	}

	queue_clear(&playlist.queue);

	playlist.current = -1;

	incrPlaylistVersion();
}

void showPlaylist(struct client *client)
{
	for (unsigned i = 0; i < queue_length(&playlist.queue); i++) {
		const struct song *song = queue_get(&playlist.queue, i);
		char *uri = song_get_uri(song);
		client_printf(client, "%i:%s\n", i, uri);
		g_free(uri);
	}
}

static void playlist_save(FILE *fp)
{
	for (unsigned i = 0; i < queue_length(&playlist.queue); i++) {
		const struct song *song = queue_get(&playlist.queue, i);
		char *uri = song_get_uri(song);
		fprintf(fp, "%i:%s\n", i, uri);
		g_free(uri);
	}
}

void savePlaylistState(FILE *fp)
{
	fprintf(fp, "%s", PLAYLIST_STATE_FILE_STATE);

	if (playlist.playing) {
		switch (getPlayerState()) {
		case PLAYER_STATE_PAUSE:
			fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_PAUSE);
			break;
		default:
			fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_PLAY);
		}
		fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_CURRENT,
			queue_order_to_position(&playlist.queue,
						playlist.current));
		fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_TIME,
		        getPlayerElapsedTime());
	} else
		fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_STOP);

	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_RANDOM,
		playlist.queue.random);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_REPEAT,
		playlist.queue.repeat);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_CROSSFADE,
	        (int)(getPlayerCrossFade()));
	fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_PLAYLIST_BEGIN);
	playlist_save(fp);
	fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_PLAYLIST_END);
}

static void loadPlaylistFromStateFile(FILE *fp, char *buffer,
				      int state, int current, int seek_time)
{
	char *temp;
	int song;

	if (!fgets(buffer, PLAYLIST_BUFFER_SIZE, fp)) {
		g_warning("No playlist in state file");
		return;
	}

	while (!g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_PLAYLIST_END)) {
		g_strchomp(buffer);

		temp = strtok(buffer, ":");
		if (temp == NULL) {
			g_warning("Malformed playlist line in state file");
			break;
		}

		song = atoi(temp);
		if (!(temp = strtok(NULL, ""))) {
			g_warning("Malformed playlist line in state file");
			break;
		}

		if (addToPlaylist(temp, NULL) == PLAYLIST_RESULT_SUCCESS
		    && current == song) {
			if (state != PLAYER_STATE_STOP) {
				playPlaylist(queue_length(&playlist.queue) - 1);
			}
			if (state == PLAYER_STATE_PAUSE) {
				playerPause();
			}
			if (state != PLAYER_STATE_STOP) {
				seekSongInPlaylist(queue_length(&playlist.queue) - 1,
						   seek_time);
			}
		}

		if (!fgets(buffer, PLAYLIST_BUFFER_SIZE, fp)) {
			g_warning("'%s' not found in state file",
				  PLAYLIST_STATE_FILE_PLAYLIST_END);
			break;
		}
	}
}

void readPlaylistState(FILE *fp)
{
	int current = -1;
	int seek_time = 0;
	int state = PLAYER_STATE_STOP;
	char buffer[PLAYLIST_BUFFER_SIZE];

	while (fgets(buffer, sizeof(buffer), fp)) {
		g_strchomp(buffer);

		if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_STATE)) {
			if (strcmp(&(buffer[strlen(PLAYLIST_STATE_FILE_STATE)]),
				   PLAYLIST_STATE_FILE_STATE_PLAY) == 0) {
				state = PLAYER_STATE_PLAY;
			} else
			    if (strcmp
				(&(buffer[strlen(PLAYLIST_STATE_FILE_STATE)]),
				 PLAYLIST_STATE_FILE_STATE_PAUSE)
				== 0) {
				state = PLAYER_STATE_PAUSE;
			}
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_TIME)) {
			seek_time =
			    atoi(&(buffer[strlen(PLAYLIST_STATE_FILE_TIME)]));
		} else
		    if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_REPEAT)) {
			if (strcmp
			    (&(buffer[strlen(PLAYLIST_STATE_FILE_REPEAT)]),
			     "1") == 0) {
				setPlaylistRepeatStatus(true);
			} else
				setPlaylistRepeatStatus(false);
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_CROSSFADE)) {
			setPlayerCrossFade(atoi
					   (&
					    (buffer
					     [strlen
					      (PLAYLIST_STATE_FILE_CROSSFADE)])));
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_RANDOM)) {
			if (strcmp
			    (&
			     (buffer
			      [strlen(PLAYLIST_STATE_FILE_RANDOM)]),
			     "1") == 0) {
				setPlaylistRandomStatus(true);
			} else
				setPlaylistRandomStatus(false);
		} else if (g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_CURRENT)) {
			current = atoi(&(buffer
					 [strlen
					  (PLAYLIST_STATE_FILE_CURRENT)]));
		} else if (g_str_has_prefix(buffer,
					    PLAYLIST_STATE_FILE_PLAYLIST_BEGIN)) {
			if (state == PLAYER_STATE_STOP)
				current = -1;
			loadPlaylistFromStateFile(fp, buffer, state,
						  current, seek_time);
		}
	}
}

static void printPlaylistSongInfo(struct client *client, unsigned song)
{
	song_print_info(client, queue_get(&playlist.queue, song));
	client_printf(client, "Pos: %u\nId: %u\n",
		      song, queue_position_to_id(&playlist.queue, song));
}

int playlistChanges(struct client *client, uint32_t version)
{
	for (unsigned i = 0; i < queue_length(&playlist.queue); i++) {
		if (queue_song_newer(&playlist.queue, i, version))
			printPlaylistSongInfo(client, i);
	}

	return 0;
}

int playlistChangesPosId(struct client *client, uint32_t version)
{
	for (unsigned i = 0; i < queue_length(&playlist.queue); i++) {
		if (queue_song_newer(&playlist.queue, i, version))
			client_printf(client, "cpos: %i\nId: %i\n",
				      i, queue_position_to_id(&playlist.queue, i));
	}

	return 0;
}

enum playlist_result
playlistInfo(struct client *client, unsigned start, unsigned end)
{
	if (end > queue_length(&playlist.queue))
		end = queue_length(&playlist.queue);

	if (start > end)
		return PLAYLIST_RESULT_BAD_RANGE;

	for (unsigned i = start; i < end; i++)
		printPlaylistSongInfo(client, i);

	return PLAYLIST_RESULT_SUCCESS;
}

static int song_id_to_position(unsigned id)
{
	return queue_id_to_position(&playlist.queue, id);
}

enum playlist_result playlistId(struct client *client, int id)
{
	int begin = 0;
	unsigned end = queue_length(&playlist.queue);

	if (id >= 0) {
		begin = song_id_to_position(id);
		if (begin < 0)
			return PLAYLIST_RESULT_NO_SUCH_SONG;

		end = begin + 1;
	}

	for (unsigned i = begin; i < end; i++)
		printPlaylistSongInfo(client, i);

	return PLAYLIST_RESULT_SUCCESS;
}

static void swapSongs(unsigned song1, unsigned song2)
{
	queue_swap(&playlist.queue, song1, song2);
}

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

static void queueNextSongInPlaylist(void)
{
	assert(playlist.queued < 0);

	if (playlist.current + 1 < (int)queue_length(&playlist.queue)) {
		playlist_queue_song_order(playlist.current + 1);
	} else if (!queue_is_empty(&playlist.queue) && playlist.queue.repeat) {
		if (playlist.queue.random) {
			unsigned current_position =
				queue_order_to_position(&playlist.queue,
							playlist.current);
			queue_shuffle_order(&playlist.queue);
			playlist.current =
				queue_position_to_order(&playlist.queue,
							current_position);
		}

		playlist_queue_song_order(0);
	}
}

static void syncPlaylistWithQueue(void)
{
	if (pc.next_song == NULL && playlist.queued != -1) {
		playlist.current = playlist.queued;
		playlist.queued = -1;

		idle_add(IDLE_PLAYER);
	}
}

static void clearPlayerQueue(void)
{
	assert(playlist.queued >= 0);

	playlist.queued = -1;

	pc_cancel();
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
	unsigned id;

	if (queue_is_full(&playlist.queue))
		return PLAYLIST_RESULT_TOO_LARGE;

	if (playlist.playing && playlist.queued >= 0 &&
	    playlist.current == (int)queue_length(&playlist.queue) - 1)
		clearPlayerQueue();

	id = queue_append(&playlist.queue, song);

	if (playlist.queue.random) {
		unsigned start;
		/*if(playlist_state==PLAYLIST_STATE_STOP) start = 0;
		   else */ if (playlist.queued >= 0)
			start = playlist.queued + 1;
		else
			start = playlist.current + 1;
		if (start < queue_length(&playlist.queue)) {
			unsigned swap = g_rand_int_range(g_rand, start,
							 queue_length(&playlist.queue));
			queue_swap_order(&playlist.queue,
					 queue_length(&playlist.queue) - 1, swap);
		}
	}

	incrPlaylistVersion();

	if (added_id)
		*added_id = id;

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result swapSongsInPlaylist(unsigned song1, unsigned song2)
{
	if (!queue_valid_position(&playlist.queue, song1) ||
	    !queue_valid_position(&playlist.queue, song2))
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist.playing && playlist.queued >= 0) {
		unsigned queuedSong = queue_order_to_position(&playlist.queue,
							      playlist.queued);
		unsigned currentSong = queue_order_to_position(&playlist.queue,
							       playlist.current);

		if (queuedSong == song1 || queuedSong == song2
		    || currentSong == song1 || currentSong == song2)
			clearPlayerQueue();
	}

	swapSongs(song1, song2);
	if (playlist.queue.random) {
		queue_swap_order(&playlist.queue,
				 queue_position_to_order(&playlist.queue,
							 song1),
				 queue_position_to_order(&playlist.queue,
							 song2));
	} else {
		if (playlist.current == (int)song1)
			playlist.current = song2;
		else if (playlist.current == (int)song2)
			playlist.current = song1;
	}

	incrPlaylistVersion();

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result swapSongsInPlaylistById(unsigned id1, unsigned id2)
{
	int song1 = song_id_to_position(id1);
	int song2 = song_id_to_position(id2);

	if (song1 < 0 || song2 < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return swapSongsInPlaylist(song1, song2);
}

enum playlist_result deleteFromPlaylist(unsigned song)
{
	unsigned songOrder;

	if (song >= queue_length(&playlist.queue))
		return PLAYLIST_RESULT_BAD_RANGE;

	songOrder = queue_position_to_order(&playlist.queue, song);

	if (playlist.playing && playlist.queued >= 0
	    && (playlist.queued == (int)songOrder ||
		playlist.current == (int)songOrder))
		clearPlayerQueue();

	if (playlist.playing && playlist.current == (int)songOrder) {
		/*if(playlist.current>=playlist.length) return playerStop(fd);
		   else return playPlaylistOrderNumber(fd,playlist.current); */
		playerWait();

		playlist.current = queue_next_order(&playlist.queue,
						    playlist.current);
		if (playlist.current == (int)songOrder)
			playlist.current = -1;

		if (playlist.current >= 0)
			/* play the song after the deleted one */
			playPlaylistOrderNumber(playlist.current);
		else
			/* no songs left to play, stop playback
			   completely */
			stopPlaylist();
	}

	if (!song_in_database(queue_get(&playlist.queue, song)))
		pc_song_deleted(queue_get(&playlist.queue, song));

	queue_delete(&playlist.queue, song);

	incrPlaylistVersion();

	if (playlist.current > (int)songOrder) {
		playlist.current--;
	}

	if (playlist.queued > (int)songOrder) {
		playlist.queued--;
	}

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result deleteFromPlaylistById(unsigned id)
{
	int song = song_id_to_position(id);
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
	g_debug("playlist: stop");
	playerWait();
	playlist.queued = -1;
	playlist.playing = false;

	if (playlist.queue.random) {
		unsigned current_position =
			queue_order_to_position(&playlist.queue,
						playlist.current);
		queue_shuffle_order(&playlist.queue);
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
		if (queue_is_empty(&playlist.queue))
			return PLAYLIST_RESULT_SUCCESS;

		if (playlist.playing) {
			playerSetPause(0);
			return PLAYLIST_RESULT_SUCCESS;
		}

		i = playlist.current >= 0
			? playlist.current
			: 0;
	} else if (!queue_valid_position(&playlist.queue, song))
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist.queue.random) {
		if (song >= 0)
			i = queue_position_to_order(&playlist.queue, song);

		if (!playlist.playing)
			playlist.current = 0;

		queue_swap_order(&playlist.queue,
				 i, playlist.current);
		i = playlist.current;
	}

	playlist_stopOnError = false;
	playlist_errorCount = 0;

	playPlaylistOrderNumber(i);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result playPlaylistById(int id)
{
	int song;

	if (id == -1) {
		return playPlaylist(id);
	}

	song = song_id_to_position(id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playPlaylist(song);
}

static void playPlaylistIfPlayerStopped(void);

void syncPlayerAndPlaylist(void)
{
	if (!playlist.playing)
		return;

	if (getPlayerState() == PLAYER_STATE_STOP)
		playPlaylistIfPlayerStopped();
	else {
		syncPlaylistWithQueue();
		if (pc.next_song == NULL)
			queueNextSongInPlaylist();
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

	playlist_stopOnError = 0;

	next_order = queue_next_order(&playlist.queue, playlist.current);
	if (next_order < 0) {
		stopPlaylist();
		return;
	}

	if (next_order == 0) {
		assert(playlist.queue.repeat);

		queue_shuffle_order(&playlist.queue);
	}

	playPlaylistOrderNumber(next_order);
}

static void playPlaylistIfPlayerStopped(void)
{
	if (getPlayerState() == PLAYER_STATE_STOP) {
		enum player_error error = getPlayerError();

		if (error == PLAYER_ERROR_NOERROR)
			playlist_errorCount = 0;
		else
			playlist_errorCount++;

		if (playlist.playing
		    && ((playlist_stopOnError && error != PLAYER_ERROR_NOERROR)
			|| error == PLAYER_ERROR_AUDIO
			|| error == PLAYER_ERROR_SYSTEM
			|| playlist_errorCount >= queue_length(&playlist.queue))) {
			stopPlaylist();
		} else
			nextSongInPlaylist();
	}
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

	if (playlist.playing &&
	    playlist.queue.repeat && playlist.queued == 0)
		clearPlayerQueue();

	playlist.queue.repeat = status;

	idle_add(IDLE_OPTIONS);
}

enum playlist_result moveSongInPlaylist(unsigned from, int to)
{
	int currentSong;

	if (!queue_valid_position(&playlist.queue, from))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((to >= 0 && to >= (int)queue_length(&playlist.queue)) ||
	    (to < 0 && abs(to) > (int)queue_length(&playlist.queue)))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((int)from == to) /* no-op */
		return PLAYLIST_RESULT_SUCCESS;

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

	if (playlist.playing && playlist.queued >= 0) {
		int queuedSong = queue_order_to_position(&playlist.queue,
							 playlist.queued);
		if (queuedSong == (int)from || queuedSong == to
		    || currentSong == (int)from || currentSong == to)
			clearPlayerQueue();
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

		/* this first if statement isn't necessary since the queue
		 * would have been cleared out if queued == from */
		if (playlist.queued == (int)from)
			playlist.queued = to;
		else if (playlist.queued > (int)from && playlist.queued <= to) {
			playlist.queued--;
		} else if (playlist.queued>= to && playlist.queued < (int)from) {
			playlist.queued++;
		}
	}

	incrPlaylistVersion();

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result moveSongInPlaylistById(unsigned id1, int to)
{
	int song = song_id_to_position(id1);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return moveSongInPlaylist(song, to);
}

static void orderPlaylist(void)
{
	if (playlist.current >= 0)
		playlist.current = queue_order_to_position(&playlist.queue,
							   playlist.current);

	if (playlist.playing && playlist.queued >= 0)
		clearPlayerQueue();

	queue_restore_order(&playlist.queue);
}

void setPlaylistRandomStatus(bool status)
{
	if (status == playlist.queue.random)
		return;

	if (playlist.queued >= 0)
		clearPlayerQueue();

	playlist.queue.random = status;

	if (playlist.queue.random) {
		int current_position = playlist.current >= 0
			? (int)queue_order_to_position(&playlist.queue,
						       playlist.current)
			: -1;

		queue_shuffle_order(&playlist.queue);

		if (current_position >= 0) {
			unsigned current_order =
				queue_position_to_order(&playlist.queue,
							current_position);
			queue_swap_order(&playlist.queue, 0, current_order);
			playlist.current = 0;
		}
	} else
		orderPlaylist();

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
		playPlaylistOrderNumber(playlist.current);
	} else {
		if (playlist.current > 0) {
			playPlaylistOrderNumber(playlist.current - 1);
		} else if (playlist.queue.repeat) {
			playPlaylistOrderNumber(queue_length(&playlist.queue) - 1);
		} else {
			playPlaylistOrderNumber(playlist.current);
		}
	}
}

void shufflePlaylist(void)
{
	unsigned i;

	if (queue_length(&playlist.queue) <= 1)
		return;

	if (playlist.playing) {
		if (playlist.queued >= 0)
			clearPlayerQueue();

		if (playlist.current >= 0)
			/* put current playing song first */
			swapSongs(0, queue_order_to_position(&playlist.queue,
							     playlist.current));

		if (playlist.queue.random) {
			playlist.current =
				queue_position_to_order(&playlist.queue, 0);
		} else
			playlist.current = 0;
		i = 1;
	} else {
		i = 0;
		playlist.current = -1;
	}

	/* shuffle the rest of the list */
	queue_shuffle_range(&playlist.queue, i,
			    queue_length(&playlist.queue));

	incrPlaylistVersion();
}

enum playlist_result savePlaylist(const char *utf8file)
{
	FILE *fp;
	char *path;

	if (!is_valid_playlist_name(utf8file))
		return PLAYLIST_RESULT_BAD_NAME;

	path = map_spl_utf8_to_fs(utf8file);
	if (path == NULL)
		return PLAYLIST_RESULT_DISABLED;

	if (g_file_test(path, G_FILE_TEST_EXISTS)) {
		g_free(path);
		return PLAYLIST_RESULT_LIST_EXISTS;
	}

	while (!(fp = fopen(path, "w")) && errno == EINTR);
	g_free(path);

	if (fp == NULL)
		return PLAYLIST_RESULT_ERRNO;

	for (unsigned i = 0; i < queue_length(&playlist.queue); i++)
		playlist_print_song(fp, queue_get(&playlist.queue, i));

	while (fclose(fp) && errno == EINTR) ;

	idle_add(IDLE_STORED_PLAYLIST);
	return PLAYLIST_RESULT_SUCCESS;
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
	unsigned i;
	int ret;

	if (!queue_valid_position(&playlist.queue, song))
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist.queue.random)
		i = queue_position_to_order(&playlist.queue, song);
	else
		i = song;

	clearPlayerError();
	playlist_stopOnError = 1;
	playlist_errorCount = 0;

	if (playlist.playing) {
		if (playlist.queued >= 0)
			clearPlayerQueue();
	} else
		playPlaylistOrderNumber(i);

	if (playlist.current != (int)i) {
		playPlaylistOrderNumber(i);
	}

	ret = playerSeek(queue_get_order(&playlist.queue, i), seek_time);
	if (ret < 0)
		return PLAYLIST_RESULT_NOT_PLAYING;

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result seekSongInPlaylistById(unsigned id, float seek_time)
{
	int song = song_id_to_position(id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return seekSongInPlaylist(song, seek_time);
}

unsigned getPlaylistSongId(unsigned song)
{
	return queue_position_to_id(&playlist.queue, song);
}

int PlaylistInfo(struct client *client, const char *utf8file, int detail)
{
	GPtrArray *list;

	if (!(list = spl_load(utf8file)))
		return -1;

	for (unsigned i = 0; i < list->len; ++i) {
		const char *temp = g_ptr_array_index(list, i);
		int wrote = 0;

		if (detail) {
			struct song *song = db_get_song(temp);
			if (song) {
				song_print_info(client, song);
				wrote = 1;
			}
		}

		if (!wrote) {
			client_printf(client, SONG_FILE "%s\n", temp);
		}
	}

	spl_free(list);
	return 0;
}

enum playlist_result loadPlaylist(const char *utf8file)
{
	GPtrArray *list;

	if (!(list = spl_load(utf8file)))
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	for (unsigned i = 0; i < list->len; ++i) {
		const char *temp = g_ptr_array_index(list, i);
		if ((addToPlaylist(temp, NULL)) != PLAYLIST_RESULT_SUCCESS) {
			/* for windows compatibility, convert slashes */
			char *temp2 = g_strdup(temp);
			char *p = temp2;
			while (*p) {
				if (*p == '\\')
					*p = '/';
				p++;
			}
			if ((addToPlaylist(temp, NULL)) != PLAYLIST_RESULT_SUCCESS) {
				g_warning("can't add file \"%s\"", temp2);
			}
			free(temp2);
		}
	}

	spl_free(list);
	return PLAYLIST_RESULT_SUCCESS;
}

void searchForSongsInPlaylist(struct client *client,
			      unsigned numItems, LocateTagItem * items)
{
	unsigned i;
	char **originalNeedles = g_malloc(numItems * sizeof(char *));

	for (i = 0; i < numItems; i++) {
		originalNeedles[i] = items[i].needle;
		items[i].needle = g_utf8_casefold(originalNeedles[i], -1);
	}

	for (i = 0; i < queue_length(&playlist.queue); i++) {
		const struct song *song = queue_get(&playlist.queue, i);

		if (strstrSearchTags(song, numItems, items))
			printPlaylistSongInfo(client, i);
	}

	for (i = 0; i < numItems; i++) {
		g_free(items[i].needle);
		items[i].needle = originalNeedles[i];
	}

	free(originalNeedles);
}

void findSongsInPlaylist(struct client *client,
			 unsigned numItems, LocateTagItem * items)
{
	for (unsigned i = 0; i < queue_length(&playlist.queue); i++) {
		const struct song *song = queue_get(&playlist.queue, i);

		if (tagItemsFoundAndMatches(song, numItems, items))
			printPlaylistSongInfo(client, i);
	}
}

/*
 * Not supporting '/' was done out of laziness, and we should really
 * strive to support it in the future.
 *
 * Not supporting '\r' and '\n' is done out of protocol limitations (and
 * arguably laziness), but bending over head over heels to modify the
 * protocol (and compatibility with all clients) to support idiots who
 * put '\r' and '\n' in filenames isn't going to happen, either.
 */
int is_valid_playlist_name(const char *utf8path)
{
	return strchr(utf8path, '/') == NULL &&
		strchr(utf8path, '\n') == NULL &&
		strchr(utf8path, '\r') == NULL;
}
