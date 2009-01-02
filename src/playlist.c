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
#include "log.h"
#include "mapper.h"
#include "path.h"
#include "state_file.h"
#include "stored_playlist.h"
#include "ack.h"
#include "idle.h"
#include "list.h"

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define PLAYLIST_STATE_STOP		0
#define PLAYLIST_STATE_PLAY		1

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
static int playlist_state = PLAYLIST_STATE_STOP;
unsigned playlist_max_length = DEFAULT_PLAYLIST_MAX_LENGTH;
static int playlist_stopOnError;
static unsigned playlist_errorCount;
static int playlist_noGoToNext;

bool playlist_saveAbsolutePaths = DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS;

static void swapOrder(int a, int b);
static void playPlaylistOrderNumber(int orderNum);
static void randomizeOrder(int start, int end);

static void incrPlaylistVersion(void)
{
	static unsigned long max = ((uint32_t) 1 << 31) - 1;
	playlist.version++;
	if (playlist.version >= max) {
		for (unsigned i = 0; i < playlist.length; i++)
			playlist.songMod[i] = 0;

		playlist.version = 1;
	}

	idle_add(IDLE_PLAYLIST);
}

void playlistVersionChange(void)
{
	for (unsigned i = 0; i < playlist.length; i++)
		playlist.songMod[i] = playlist.version;

	incrPlaylistVersion();
}

static void incrPlaylistCurrent(void)
{
	if (playlist.current < 0)
		return;

	if (playlist.current >= (int)playlist.length - 1) {
		if (playlist.repeat)
			playlist.current = 0;
		else
			playlist.current = -1;
	} else
		playlist.current++;
}

void initPlaylist(void)
{
	char *test;
	ConfigParam *param;

	g_rand = g_rand_new();

	playlist.length = 0;
	playlist.repeat = false;
	playlist.version = 1;
	playlist.random = false;
	playlist.queued = -1;
	playlist.current = -1;

	param = getConfigParam(CONF_MAX_PLAYLIST_LENGTH);

	if (param) {
		playlist_max_length = strtol(param->value, &test, 10);
		if (*test != '\0') {
			FATAL("max playlist length \"%s\" is not an integer, "
			      "line %i\n", param->value, param->line);
		}
	}

	playlist_saveAbsolutePaths = getBoolConfigParam(
	                                         CONF_SAVE_ABSOLUTE_PATHS, 1);
	if (playlist_saveAbsolutePaths == CONF_BOOL_UNSET)
		playlist_saveAbsolutePaths =
		                         DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS;

	playlist.songs = g_malloc(sizeof(playlist.songs[0]) *
				  playlist_max_length);
	playlist.songMod = g_malloc(sizeof(playlist.songMod[0]) *
				    playlist_max_length);
	playlist.order = g_malloc(sizeof(playlist.order[0]) *
				  playlist_max_length);
	playlist.idToPosition = g_malloc(sizeof(playlist.idToPosition[0]) *
					 playlist_max_length *
					 PLAYLIST_HASH_MULT);
	playlist.positionToId = g_malloc(sizeof(playlist.positionToId[0]) *
					 playlist_max_length);

	memset(playlist.songs, 0, sizeof(char *) * playlist_max_length);

	for (unsigned i = 0; i < playlist_max_length * PLAYLIST_HASH_MULT;
	     i++) {
		playlist.idToPosition[i] = -1;
	}
}

static unsigned getNextId(void)
{
	static unsigned cur = (unsigned)-1;

	do {
		cur++;
		if (cur >= playlist_max_length * PLAYLIST_HASH_MULT) {
			cur = 0;
		}
	} while (playlist.idToPosition[cur] != -1);

	return cur;
}

void finishPlaylist(void)
{
	for (unsigned i = 0; i < playlist.length; i++)
		if (!song_in_database(playlist.songs[i]))
			song_free(playlist.songs[i]);

	playlist.length = 0;

	free(playlist.songs);
	playlist.songs = NULL;
	free(playlist.songMod);
	playlist.songMod = NULL;
	free(playlist.order);
	playlist.order = NULL;
	free(playlist.idToPosition);
	playlist.idToPosition = NULL;
	free(playlist.positionToId);
	playlist.positionToId = NULL;

	g_rand_free(g_rand);
	g_rand = NULL;
}

void clearPlaylist(void)
{
	stopPlaylist();

	for (unsigned i = 0; i < playlist.length; i++) {
		if (!song_in_database(playlist.songs[i])) {
			pc_song_deleted(playlist.songs[i]);
			song_free(playlist.songs[i]);
		}

		playlist.idToPosition[playlist.positionToId[i]] = -1;
		playlist.songs[i] = NULL;
	}
	playlist.length = 0;
	playlist.current = -1;

	incrPlaylistVersion();
}

void showPlaylist(struct client *client)
{
	char path_max_tmp[MPD_PATH_MAX];

	for (unsigned i = 0; i < playlist.length; i++)
		client_printf(client, "%i:%s\n", i,
			      song_get_url(playlist.songs[i], path_max_tmp));
}

static void playlist_save(FILE *fp)
{
	char path_max_tmp[MPD_PATH_MAX];

	for (unsigned i = 0; i < playlist.length; i++)
		fprintf(fp, "%i:%s\n", i,
			song_get_url(playlist.songs[i], path_max_tmp));
}

void savePlaylistState(FILE *fp)
{
	fprintf(fp, "%s", PLAYLIST_STATE_FILE_STATE);
	switch (playlist_state) {
	case PLAYLIST_STATE_PLAY:
		switch (getPlayerState()) {
		case PLAYER_STATE_PAUSE:
			fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_PAUSE);
			break;
		default:
			fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_PLAY);
		}
		fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_CURRENT,
		        playlist.order[playlist.current]);
		fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_TIME,
		        getPlayerElapsedTime());
		break;
	default:
		fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_STATE_STOP);
		break;
	}
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_RANDOM, playlist.random);
	fprintf(fp, "%s%i\n", PLAYLIST_STATE_FILE_REPEAT, playlist.repeat);
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

	if (!fgets(buffer, PLAYLIST_BUFFER_SIZE, fp))
		state_file_fatal();
	while (!g_str_has_prefix(buffer, PLAYLIST_STATE_FILE_PLAYLIST_END)) {
		g_strchomp(buffer);

		temp = strtok(buffer, ":");
		if (temp == NULL)
			state_file_fatal();
		song = atoi(temp);
		if (!(temp = strtok(NULL, "")))
			state_file_fatal();
		if (addToPlaylist(temp, NULL) == PLAYLIST_RESULT_SUCCESS
		    && current == song) {
			if (state != PLAYER_STATE_STOP) {
				playPlaylist(playlist.length - 1, 0);
			}
			if (state == PLAYER_STATE_PAUSE) {
				playerPause();
			}
			if (state != PLAYER_STATE_STOP) {
				seekSongInPlaylist(playlist.length - 1,
						   seek_time);
			}
		}

		if (!fgets(buffer, PLAYLIST_BUFFER_SIZE, fp))
			state_file_fatal();
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
			if (strlen(buffer) ==
			    strlen(PLAYLIST_STATE_FILE_CURRENT))
				state_file_fatal();
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
	song_print_info(client, playlist.songs[song]);
	client_printf(client, "Pos: %u\nId: %u\n",
		      song, playlist.positionToId[song]);
}

int playlistChanges(struct client *client, uint32_t version)
{
	for (unsigned i = 0; i < playlist.length; i++) {
		if (version > playlist.version ||
		    playlist.songMod[i] >= version ||
		    playlist.songMod[i] == 0) {
			printPlaylistSongInfo(client, i);
		}
	}

	return 0;
}

int playlistChangesPosId(struct client *client, uint32_t version)
{
	for (unsigned i = 0; i < playlist.length; i++) {
		if (version > playlist.version ||
		    playlist.songMod[i] >= version ||
		    playlist.songMod[i] == 0) {
			client_printf(client, "cpos: %i\nId: %i\n",
				      i, playlist.positionToId[i]);
		}
	}

	return 0;
}

enum playlist_result playlistInfo(struct client *client, int song)
{
	unsigned begin = 0;
	unsigned end = playlist.length;

	if (song >= 0) {
		begin = song;
		end = song + 1;
	}
	if (song >= (int)playlist.length)
		return PLAYLIST_RESULT_BAD_RANGE;

	for (unsigned i = begin; i < end; i++)
		printPlaylistSongInfo(client, i);

	return PLAYLIST_RESULT_SUCCESS;
}

static int song_id_to_position(unsigned id)
{
	if (id >= PLAYLIST_HASH_MULT*playlist_max_length)
		return -1;

	assert(playlist.idToPosition[id] >= -1);
	assert(playlist.idToPosition[id] < (int)playlist.length);

	return playlist.idToPosition[id];
}

enum playlist_result playlistId(struct client *client, int id)
{
	int begin = 0;
	unsigned end = playlist.length;

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
	struct song *sTemp;
	unsigned iTemp;

	sTemp = playlist.songs[song1];
	playlist.songs[song1] = playlist.songs[song2];
	playlist.songs[song2] = sTemp;

	playlist.songMod[song1] = playlist.version;
	playlist.songMod[song2] = playlist.version;

	playlist.idToPosition[playlist.positionToId[song1]] = song2;
	playlist.idToPosition[playlist.positionToId[song2]] = song1;

	iTemp = playlist.positionToId[song1];
	playlist.positionToId[song1] = playlist.positionToId[song2];
	playlist.positionToId[song2] = iTemp;
}

static void queueNextSongInPlaylist(void)
{
	char path_max_tmp[MPD_PATH_MAX];

	if (playlist.current < (int)playlist.length - 1) {
		playlist.queued = playlist.current + 1;
		DEBUG("playlist: queue song %i:\"%s\"\n",
		      playlist.queued,
		      song_get_url(playlist.
				   songs[playlist.order[playlist.queued]],
				   path_max_tmp));
		queueSong(playlist.songs[playlist.order[playlist.queued]]);
	} else if (playlist.length && playlist.repeat) {
		if (playlist.length > 1 && playlist.random) {
			randomizeOrder(0, playlist.length - 1);
		}
		playlist.queued = 0;
		DEBUG("playlist: queue song %i:\"%s\"\n",
		      playlist.queued,
		      song_get_url(playlist.
				   songs[playlist.order[playlist.queued]],
				   path_max_tmp));
		queueSong(playlist.songs[playlist.order[playlist.queued]]);
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

	if (isRemoteUrl(url))
		return song_remote_new(url);

	return NULL;
}

enum playlist_result addToPlaylist(const char *url, unsigned *added_id)
{
	struct song *song;

	DEBUG("add to playlist: %s\n", url);

	song = song_by_url(url);
	if (song == NULL)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return addSongToPlaylist(song, added_id);
}

enum playlist_result
addSongToPlaylist(struct song *song, unsigned *added_id)
{
	unsigned id;

	if (playlist.length == playlist_max_length)
		return PLAYLIST_RESULT_TOO_LARGE;

	if (playlist_state == PLAYLIST_STATE_PLAY && playlist.queued >= 0 &&
	    playlist.current == (int)playlist.length - 1)
		clearPlayerQueue();

	id = getNextId();

	playlist.songs[playlist.length] = song;
	playlist.songMod[playlist.length] = playlist.version;
	playlist.order[playlist.length] = playlist.length;
	playlist.positionToId[playlist.length] = id;
	playlist.idToPosition[playlist.positionToId[playlist.length]] =
	    playlist.length;
	playlist.length++;

	if (playlist.random) {
		unsigned start;
		/*if(playlist_state==PLAYLIST_STATE_STOP) start = 0;
		   else */ if (playlist.queued >= 0)
			start = playlist.queued + 1;
		else
			start = playlist.current + 1;
		if (start < playlist.length) {
			unsigned swap = g_rand_int_range(g_rand, start,
							 playlist.length);
			swapOrder(playlist.length - 1, swap);
		}
	}

	incrPlaylistVersion();

	if (added_id)
		*added_id = id;

	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result swapSongsInPlaylist(unsigned song1, unsigned song2)
{
	if (song1 >= playlist.length || song2 >= playlist.length)
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist_state == PLAYLIST_STATE_PLAY && playlist.queued >= 0) {
		unsigned queuedSong = playlist.order[playlist.queued];
		unsigned currentSong = playlist.order[playlist.current];

		if (queuedSong == song1 || queuedSong == song2
		    || currentSong == song1 || currentSong == song2)
			clearPlayerQueue();
	}

	swapSongs(song1, song2);
	if (playlist.random) {
		unsigned i, k;
		int j = -1;
		for (i = 0; playlist.order[i] != song1; i++) {
			if (playlist.order[i] == song2)
				j = i;
		}
		k = i;
		for (; j == -1; i++)
			if (playlist.order[i] == song2)
				j = i;
		swapOrder(k, j);
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

#define moveSongFromTo(from, to) { \
	playlist.idToPosition[playlist.positionToId[from]] = to; \
	playlist.positionToId[to] = playlist.positionToId[from]; \
	playlist.songs[to] = playlist.songs[from]; \
	playlist.songMod[to] = playlist.version; \
}

enum playlist_result deleteFromPlaylist(unsigned song)
{
	unsigned i;
	unsigned songOrder;

	if (song >= playlist.length)
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist_state == PLAYLIST_STATE_PLAY && playlist.queued >= 0
	    && (playlist.order[playlist.queued] == song
		|| playlist.order[playlist.current] == song))
		clearPlayerQueue();

	if (!song_in_database(playlist.songs[song])) {
		pc_song_deleted(playlist.songs[song]);
		song_free(playlist.songs[song]);
	}

	playlist.idToPosition[playlist.positionToId[song]] = -1;

	/* delete song from songs array */
	for (i = song; i < playlist.length - 1; i++) {
		moveSongFromTo(i + 1, i);
	}
	/* now find it in the order array */
	for (i = 0; i < playlist.length - 1; i++) {
		if (playlist.order[i] == song)
			break;
	}
	songOrder = i;
	/* delete the entry from the order array */
	for (; i < playlist.length - 1; i++)
		playlist.order[i] = playlist.order[i + 1];
	/* readjust values in the order array */
	for (i = 0; i < playlist.length - 1; i++) {
		if (playlist.order[i] > song)
			playlist.order[i]--;
	}
	/* now take care of other misc stuff */
	playlist.songs[playlist.length - 1] = NULL;
	playlist.length--;

	incrPlaylistVersion();

	if (playlist_state != PLAYLIST_STATE_STOP
	    && playlist.current == (int)songOrder) {
		/*if(playlist.current>=playlist.length) return playerStop(fd);
		   else return playPlaylistOrderNumber(fd,playlist.current); */
		playerWait();
		playlist_noGoToNext = 1;
	}

	if (playlist.current > (int)songOrder) {
		playlist.current--;
	} else if (playlist.current >= (int)playlist.length) {
		incrPlaylistCurrent();
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
	if (NULL == playlist.songs)
		return;

	for (unsigned i = 0; i < playlist.length; i++)
		if (song == playlist.songs[i])
			deleteFromPlaylist(i);

	pc_song_deleted(song);
}

void stopPlaylist(void)
{
	DEBUG("playlist: stop\n");
	playerWait();
	playlist.queued = -1;
	playlist_state = PLAYLIST_STATE_STOP;
	playlist_noGoToNext = 0;
	if (playlist.random)
		randomizeOrder(0, playlist.length - 1);
}

static void playPlaylistOrderNumber(int orderNum)
{
	char path_max_tmp[MPD_PATH_MAX];

	playlist_state = PLAYLIST_STATE_PLAY;
	playlist_noGoToNext = 0;
	playlist.queued = -1;

	DEBUG("playlist: play %i:\"%s\"\n", orderNum,
	      song_get_url(playlist.songs[playlist.order[orderNum]],
			   path_max_tmp));

	playerPlay(playlist.songs[playlist.order[orderNum]]);
	playlist.current = orderNum;
}

enum playlist_result playPlaylist(int song, int stopOnError)
{
	unsigned i = song;

	clearPlayerError();

	if (song == -1) {
		if (playlist.length == 0)
			return PLAYLIST_RESULT_SUCCESS;

		if (playlist_state == PLAYLIST_STATE_PLAY) {
			playerSetPause(0);
			return PLAYLIST_RESULT_SUCCESS;
		}
		if (playlist.current >= 0 &&
		    playlist.current < (int)playlist.length) {
			i = playlist.current;
		} else {
			i = 0;
		}
	} else if (song < 0 || song >= (int)playlist.length) {
		return PLAYLIST_RESULT_BAD_RANGE;
	}

	if (playlist.random) {
		if (song == -1 && playlist_state == PLAYLIST_STATE_PLAY) {
			randomizeOrder(0, playlist.length - 1);
		} else {
			if (song >= 0)
				for (i = 0; song != (int)playlist.order[i];
				     i++) ;
			if (playlist_state == PLAYLIST_STATE_STOP) {
				playlist.current = 0;
			}
			swapOrder(i, playlist.current);
			i = playlist.current;
		}
	}

	playlist_stopOnError = stopOnError;
	playlist_errorCount = 0;

	playPlaylistOrderNumber(i);
	return PLAYLIST_RESULT_SUCCESS;
}

enum playlist_result playPlaylistById(int id, int stopOnError)
{
	int song;

	if (id == -1) {
		return playPlaylist(id, stopOnError);
	}

	song = song_id_to_position(id);
	if (song < 0)
		return PLAYLIST_RESULT_NO_SUCH_SONG;

	return playPlaylist(song, stopOnError);
}

static void syncCurrentPlayerDecodeMetadata(void)
{
	struct song *song;
	int songNum;

	if (playlist_state != PLAYLIST_STATE_PLAY)
		return;

	songNum = playlist.order[playlist.current];
	song = playlist.songs[songNum];

	if (song != playlist.prev_song) {
		/* song change: initialize playlist.prev_{song,tag} */

		playlist.prev_song = song;
		playlist.prev_tag = song->tag;
	} else if (song->tag != playlist.prev_tag) {
		/* tag change: update playlist */

		playlist.songMod[songNum] = playlist.version;
		incrPlaylistVersion();

		playlist.prev_tag = song->tag;
	}
}

void syncPlayerAndPlaylist(void)
{
	if (playlist_state != PLAYLIST_STATE_PLAY)
		return;

	if (getPlayerState() == PLAYER_STATE_STOP)
		playPlaylistIfPlayerStopped();
	else {
		syncPlaylistWithQueue();
		if (pc.next_song == NULL)
			queueNextSongInPlaylist();
	}

	syncCurrentPlayerDecodeMetadata();
}

static void currentSongInPlaylist(void)
{
	if (playlist_state != PLAYLIST_STATE_PLAY)
		return;

	playlist_stopOnError = 0;

	syncPlaylistWithQueue();

	if (playlist.current >= 0 && playlist.current < (int)playlist.length)
		playPlaylistOrderNumber(playlist.current);
	else
		stopPlaylist();
}

void nextSongInPlaylist(void)
{
	if (playlist_state != PLAYLIST_STATE_PLAY)
		return;

	syncPlaylistWithQueue();

	playlist_stopOnError = 0;

	if (playlist.current < (int)playlist.length - 1) {
		playPlaylistOrderNumber(playlist.current + 1);
	} else if (playlist.length && playlist.repeat) {
		if (playlist.random)
			randomizeOrder(0, playlist.length - 1);
		playPlaylistOrderNumber(0);
	} else {
		incrPlaylistCurrent();
		stopPlaylist();
	}
}

void playPlaylistIfPlayerStopped(void)
{
	if (getPlayerState() == PLAYER_STATE_STOP) {
		int error = getPlayerError();

		if (error == PLAYER_ERROR_NOERROR)
			playlist_errorCount = 0;
		else
			playlist_errorCount++;

		if (playlist_state == PLAYLIST_STATE_PLAY
		    && ((playlist_stopOnError && error != PLAYER_ERROR_NOERROR)
			|| error == PLAYER_ERROR_AUDIO
			|| error == PLAYER_ERROR_SYSTEM
			|| playlist_errorCount >= playlist.length)) {
			stopPlaylist();
		} else if (playlist_noGoToNext)
			currentSongInPlaylist();
		else
			nextSongInPlaylist();
	}
}

bool getPlaylistRepeatStatus(void)
{
	return playlist.repeat;
}

bool getPlaylistRandomStatus(void)
{
	return playlist.random;
}

void setPlaylistRepeatStatus(bool status)
{
	if (playlist_state == PLAYLIST_STATE_PLAY &&
	    playlist.repeat && !status && playlist.queued == 0)
		clearPlayerQueue();

	playlist.repeat = status;

	idle_add(IDLE_OPTIONS);
}

enum playlist_result moveSongInPlaylist(unsigned from, int to)
{
	unsigned i;
	struct song *tmpSong;
	unsigned tmpId;
	unsigned currentSong;

	if (from >= playlist.length)
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((to >= 0 && to >= (int)playlist.length) ||
	    (to < 0 && abs(to) > (int)playlist.length))
		return PLAYLIST_RESULT_BAD_RANGE;

	if ((int)from == to) /* no-op */
		return PLAYLIST_RESULT_SUCCESS;

	/*
	 * (to < 0) => move to offset from current song
	 * (-playlist.length == to) => move to position BEFORE current song
	 */
	currentSong = playlist.order[playlist.current];
	if (to < 0 && playlist.current >= 0) {
		if (currentSong == from)
			/* no-op, can't be moved to offset of itself */
			return PLAYLIST_RESULT_SUCCESS;
		to = (currentSong + abs(to)) % playlist.length;
	}

	if (playlist_state == PLAYLIST_STATE_PLAY && playlist.queued >= 0) {
		int queuedSong = playlist.order[playlist.queued];
		if (queuedSong == (int)from || queuedSong == to
		    || currentSong == from || (int)currentSong == to)
			clearPlayerQueue();
	}

	tmpSong = playlist.songs[from];
	tmpId = playlist.positionToId[from];
	/* move songs to one less in from->to */
	for (i = from; (int)i < to; i++) {
		moveSongFromTo(i + 1, i);
	}
	/* move songs to one more in to->from */
	for (i = from; (int)i > to; i--) {
		moveSongFromTo(i - 1, i);
	}
	/* put song at _to_ */
	playlist.idToPosition[tmpId] = to;
	playlist.positionToId[to] = tmpId;
	playlist.songs[to] = tmpSong;
	playlist.songMod[to] = playlist.version;
	/* now deal with order */
	if (playlist.random) {
		for (i = 0; i < playlist.length; i++) {
			if (playlist.order[i] > from &&
			    (int)playlist.order[i] <= to) {
				playlist.order[i]--;
			} else if (playlist.order[i] < from &&
				   (int)playlist.order[i] >= to) {
				playlist.order[i]++;
			} else if (from == playlist.order[i]) {
				playlist.order[i] = to;
			}
		}
	}
	else
	{
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
	unsigned i;

	if (playlist.current >= 0 && playlist.current < (int)playlist.length)
		playlist.current = playlist.order[playlist.current];

	if (playlist_state == PLAYLIST_STATE_PLAY && playlist.queued >= 0)
		clearPlayerQueue();

	for (i = 0; i < playlist.length; i++) {
		playlist.order[i] = i;
	}

}

static void swapOrder(int a, int b)
{
	int bak = playlist.order[a];
	playlist.order[a] = playlist.order[b];
	playlist.order[b] = bak;
}

static void randomizeOrder(int start, int end)
{
	int i;
	int ri;

	DEBUG("playlist: randomize from %i to %i\n", start, end);

	if (playlist_state == PLAYLIST_STATE_PLAY &&
	    playlist.queued >= start && playlist.queued <= end)
		clearPlayerQueue();

	for (i = start; i <= end; i++) {
		ri = g_rand_int_range(g_rand, start, end + 1);
		if (ri == playlist.current)
			playlist.current = i;
		else if (i == playlist.current)
			playlist.current = ri;
		swapOrder(i, ri);
	}
}

void setPlaylistRandomStatus(bool status)
{
	if (status == playlist.random)
		return;

	playlist.random = status;

	if (playlist.random) {
		/*if(playlist_state==PLAYLIST_STATE_PLAY) {
		  randomizeOrder(playlist.current+1,
		  playlist.length-1);
		  }
		  else */ randomizeOrder(0, playlist.length - 1);
		if (playlist.current >= 0 &&
		    playlist.current < (int)playlist.length) {
			swapOrder(playlist.current, 0);
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

	if (playlist_state != PLAYLIST_STATE_PLAY)
		return;

	syncPlaylistWithQueue();

	if (diff && getPlayerElapsedTime() > PLAYLIST_PREV_UNLESS_ELAPSED) {
		playPlaylistOrderNumber(playlist.current);
	} else {
		if (playlist.current > 0) {
			playPlaylistOrderNumber(playlist.current - 1);
		} else if (playlist.repeat) {
			playPlaylistOrderNumber(playlist.length - 1);
		} else {
			playPlaylistOrderNumber(playlist.current);
		}
	}
}

void shufflePlaylist(void)
{
	unsigned i;
	int ri;

	if (playlist.length > 1) {
		if (playlist_state == PLAYLIST_STATE_PLAY) {
			if (playlist.queued >= 0)
				clearPlayerQueue();

			/* put current playing song first */
			swapSongs(0, playlist.order[playlist.current]);
			if (playlist.random) {
				int j;
				for (j = 0; 0 != playlist.order[j]; j++) ;
				playlist.current = j;
			} else
				playlist.current = 0;
			i = 1;
		} else {
			i = 0;
			playlist.current = -1;
		}
		/* shuffle the rest of the list */
		for (; i < playlist.length; i++) {
			ri = g_rand_int_range(g_rand, 1, playlist.length);
			swapSongs(i, ri);
		}

		incrPlaylistVersion();
	}
}

enum playlist_result savePlaylist(const char *utf8file)
{
	FILE *fp;
	char *path;

	if (!is_valid_playlist_name(utf8file))
		return PLAYLIST_RESULT_BAD_NAME;

	path = map_spl_utf8_to_fs(utf8file);
	if (g_file_test(path, G_FILE_TEST_EXISTS)) {
		g_free(path);
		return PLAYLIST_RESULT_LIST_EXISTS;
	}

	while (!(fp = fopen(path, "w")) && errno == EINTR);
	g_free(path);

	if (fp == NULL)
		return PLAYLIST_RESULT_ERRNO;

	for (unsigned i = 0; i < playlist.length; i++)
		playlist_print_song(fp, playlist.songs[i]);

	while (fclose(fp) && errno == EINTR) ;

	idle_add(IDLE_STORED_PLAYLIST);
	return PLAYLIST_RESULT_SUCCESS;
}

int getPlaylistCurrentSong(void)
{
	if (playlist.current >= 0 &&
	    playlist.current < (int)playlist.length) {
		return playlist.order[playlist.current];
	}

	return -1;
}

unsigned long getPlaylistVersion(void)
{
	return playlist.version;
}

int getPlaylistLength(void)
{
	return playlist.length;
}

enum playlist_result seekSongInPlaylist(unsigned song, float seek_time)
{
	unsigned i;
	int ret;

	if (song >= playlist.length)
		return PLAYLIST_RESULT_BAD_RANGE;

	if (playlist.random)
		for (i = 0; song != playlist.order[i]; i++) ;
	else
		i = song;

	clearPlayerError();
	playlist_stopOnError = 1;
	playlist_errorCount = 0;

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		if (playlist.queued >= 0)
			clearPlayerQueue();
	} else
		playPlaylistOrderNumber(i);

	if (playlist.current != (int)i) {
		playPlaylistOrderNumber(i);
	}

	ret = playerSeek(playlist.songs[playlist.order[i]], seek_time);
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
	return playlist.positionToId[song];
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

enum playlist_result loadPlaylist(struct client *client, const char *utf8file)
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
				command_error(client, ACK_ERROR_PLAYLIST_LOAD,
					      "can't add file \"%s\"", temp2);
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

	for (i = 0; i < playlist.length; i++) {
		if (strstrSearchTags(playlist.songs[i], numItems, items))
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
	for (unsigned i = 0; i < playlist.length; i++) {
		if (tagItemsFoundAndMatches(playlist.songs[i], numItems, items))
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
