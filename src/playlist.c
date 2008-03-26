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
#include "player.h"
#include "command.h"
#include "ls.h"
#include "tag.h"
#include "conf.h"
#include "directory.h"
#include "log.h"
#include "path.h"
#include "utils.h"
#include "sig_handlers.h"
#include "state_file.h"
#include "storedPlaylist.h"
#include "os_compat.h"

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
#define DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS	0

static Playlist playlist;
static int playlist_state = PLAYLIST_STATE_STOP;
int playlist_max_length = DEFAULT_PLAYLIST_MAX_LENGTH;
static int playlist_stopOnError;
static int playlist_errorCount;
static int playlist_queueError;
static int playlist_noGoToNext;

int playlist_saveAbsolutePaths = DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS;

static void swapOrder(int a, int b);
static int playPlaylistOrderNumber(int fd, int orderNum);
static void randomizeOrder(int start, int end);

static void incrPlaylistVersion(void)
{
	static unsigned long max = ((mpd_uint32) 1 << 31) - 1;
	playlist.version++;
	if (playlist.version >= max) {
		int i;

		for (i = 0; i < playlist.length; i++) {
			playlist.songMod[i] = 0;
		}

		playlist.version = 1;
	}
}

void playlistVersionChange(void)
{
	int i;

	for (i = 0; i < playlist.length; i++) {
		playlist.songMod[i] = playlist.version;
	}

	incrPlaylistVersion();
}

static void incrPlaylistCurrent(void)
{
	if (playlist.current < 0)
		return;

	if (playlist.current >= playlist.length - 1) {
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
	int i;
	ConfigParam *param;

	playlist.length = 0;
	playlist.repeat = 0;
	playlist.version = 1;
	playlist.random = 0;
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

	playlist.songs = xmalloc(sizeof(Song *) * playlist_max_length);
	playlist.songMod = xmalloc(sizeof(mpd_uint32) * playlist_max_length);
	playlist.order = xmalloc(sizeof(int) * playlist_max_length);
	playlist.idToPosition = xmalloc(sizeof(int) * playlist_max_length *
				       PLAYLIST_HASH_MULT);
	playlist.positionToId = xmalloc(sizeof(int) * playlist_max_length);

	memset(playlist.songs, 0, sizeof(char *) * playlist_max_length);

	srandom(time(NULL));

	for (i = 0; i < playlist_max_length * PLAYLIST_HASH_MULT; i++) {
		playlist.idToPosition[i] = -1;
	}
}

static int getNextId(void)
{
	static int cur = -1;

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
	int i;
	for (i = 0; i < playlist.length; i++) {
		if (playlist.songs[i]->type == SONG_TYPE_URL) {
			freeJustSong(playlist.songs[i]);
		}
	}

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
}

int clearPlaylist(int fd)
{
	int i;

	if (stopPlaylist(fd) < 0)
		return -1;

	for (i = 0; i < playlist.length; i++) {
		if (playlist.songs[i]->type == SONG_TYPE_URL) {
			freeJustSong(playlist.songs[i]);
		}
		playlist.idToPosition[playlist.positionToId[i]] = -1;
		playlist.songs[i] = NULL;
	}
	playlist.length = 0;
	playlist.current = -1;

	incrPlaylistVersion();

	return 0;
}

int clearStoredPlaylist(int fd, char *utf8file)
{
	return removeAllFromStoredPlaylistByPath(fd, utf8file);
}

int showPlaylist(int fd)
{
	int i;
	char path_max_tmp[MPD_PATH_MAX];

	for (i = 0; i < playlist.length; i++) {
		fdprintf(fd, "%i:%s\n", i,
		         get_song_url(path_max_tmp, playlist.songs[i]));
	}

	return 0;
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
	fflush(fp);
	showPlaylist(fileno(fp));
	fprintf(fp, "%s\n", PLAYLIST_STATE_FILE_PLAYLIST_END);
}

static void loadPlaylistFromStateFile(FILE *fp, char *buffer,
				      int state, int current, int seek_time)
{
	char *temp;
	int song;

	if (!myFgets(buffer, PLAYLIST_BUFFER_SIZE, fp))
		state_file_fatal();
	while (strcmp(buffer, PLAYLIST_STATE_FILE_PLAYLIST_END)) {
		song = atoi(strtok(buffer, ":"));
		if (!(temp = strtok(NULL, "")))
			state_file_fatal();
		if (!addToPlaylist(STDERR_FILENO, temp, NULL)
		    && current == song) {
			if (state != PLAYER_STATE_STOP) {
				playPlaylist(STDERR_FILENO,
					     playlist.length - 1, 0);
			}
			if (state == PLAYER_STATE_PAUSE) {
				playerPause(STDERR_FILENO);
			}
			if (state != PLAYER_STATE_STOP) {
				seekSongInPlaylist(STDERR_FILENO,
						   playlist.length - 1,
						   seek_time);
			}
		}
		if (!myFgets(buffer, PLAYLIST_BUFFER_SIZE, fp))
			state_file_fatal();
	}
}

void readPlaylistState(FILE *fp)
{
	int current = -1;
	int seek_time = 0;
	int state = PLAYER_STATE_STOP;
	char buffer[PLAYLIST_BUFFER_SIZE];

	while (myFgets(buffer, PLAYLIST_BUFFER_SIZE, fp)) {
		if (strncmp(buffer, PLAYLIST_STATE_FILE_STATE,
			    strlen(PLAYLIST_STATE_FILE_STATE)) == 0) {
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
		} else if (strncmp(buffer, PLAYLIST_STATE_FILE_TIME,
				   strlen(PLAYLIST_STATE_FILE_TIME)) == 0) {
			seek_time =
			    atoi(&(buffer[strlen(PLAYLIST_STATE_FILE_TIME)]));
		} else
		    if (strncmp
			(buffer, PLAYLIST_STATE_FILE_REPEAT,
			 strlen(PLAYLIST_STATE_FILE_REPEAT)) == 0) {
			if (strcmp
			    (&(buffer[strlen(PLAYLIST_STATE_FILE_REPEAT)]),
			     "1") == 0) {
				setPlaylistRepeatStatus(STDERR_FILENO, 1);
			} else
				setPlaylistRepeatStatus(STDERR_FILENO, 0);
		} else
		    if (strncmp
			(buffer, PLAYLIST_STATE_FILE_CROSSFADE,
			 strlen(PLAYLIST_STATE_FILE_CROSSFADE)) == 0) {
			setPlayerCrossFade(atoi
					   (&
					    (buffer
					     [strlen
					      (PLAYLIST_STATE_FILE_CROSSFADE)])));
		} else
		    if (strncmp
			(buffer, PLAYLIST_STATE_FILE_RANDOM,
			 strlen(PLAYLIST_STATE_FILE_RANDOM)) == 0) {
			if (strcmp
			    (&
			     (buffer
			      [strlen(PLAYLIST_STATE_FILE_RANDOM)]),
			     "1") == 0) {
				setPlaylistRandomStatus(STDERR_FILENO, 1);
			} else
				setPlaylistRandomStatus(STDERR_FILENO, 0);
		} else if (strncmp(buffer, PLAYLIST_STATE_FILE_CURRENT,
				   strlen(PLAYLIST_STATE_FILE_CURRENT))
			   == 0) {
			if (strlen(buffer) ==
			    strlen(PLAYLIST_STATE_FILE_CURRENT))
				state_file_fatal();
			current = atoi(&(buffer
					 [strlen
					  (PLAYLIST_STATE_FILE_CURRENT)]));
		} else
		    if (strncmp
			(buffer, PLAYLIST_STATE_FILE_PLAYLIST_BEGIN,
			 strlen(PLAYLIST_STATE_FILE_PLAYLIST_BEGIN)
			) == 0) {
			if (state == PLAYER_STATE_STOP)
				current = -1;
			loadPlaylistFromStateFile(fp, buffer, state,
						  current, seek_time);
		}
	}
}

static void printPlaylistSongInfo(int fd, int song)
{
	printSongInfo(fd, playlist.songs[song]);
	fdprintf(fd, "Pos: %i\nId: %i\n", song, playlist.positionToId[song]);
}

int playlistChanges(int fd, mpd_uint32 version)
{
	int i;

	for (i = 0; i < playlist.length; i++) {
		if (version > playlist.version ||
		    playlist.songMod[i] >= version ||
		    playlist.songMod[i] == 0) {
			printPlaylistSongInfo(fd, i);
		}
	}

	return 0;
}

int playlistChangesPosId(int fd, mpd_uint32 version)
{
	int i;

	for (i = 0; i < playlist.length; i++) {
		if (version > playlist.version ||
		    playlist.songMod[i] >= version ||
		    playlist.songMod[i] == 0) {
			fdprintf(fd, "cpos: %i\nId: %i\n",
			         i, playlist.positionToId[i]);
		}
	}

	return 0;
}

int playlistInfo(int fd, int song)
{
	int i;
	int begin = 0;
	int end = playlist.length;

	if (song >= 0) {
		begin = song;
		end = song + 1;
	}
	if (song >= playlist.length) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "song doesn't exist: \"%i\"", song);
		return -1;
	}

	for (i = begin; i < end; i++)
		printPlaylistSongInfo(fd, i);

	return 0;
}

# define checkSongId(id) { \
	if(id < 0 || id >= PLAYLIST_HASH_MULT*playlist_max_length || \
			playlist.idToPosition[id] == -1 ) \
	{ \
		commandError(fd, ACK_ERROR_NO_EXIST, \
			"song id doesn't exist: \"%i\"", id); \
		return -1; \
	} \
}

int playlistId(int fd, int id)
{
	int i;
	int begin = 0;
	int end = playlist.length;

	if (id >= 0) {
		checkSongId(id);
		begin = playlist.idToPosition[id];
		end = begin + 1;
	}

	for (i = begin; i < end; i++)
		printPlaylistSongInfo(fd, i);

	return 0;
}

static void swapSongs(int song1, int song2)
{
	Song *sTemp;
	int iTemp;

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

	if (playlist.current < playlist.length - 1) {
		playlist.queued = playlist.current + 1;
		DEBUG("playlist: queue song %i:\"%s\"\n",
		      playlist.queued,
		      get_song_url(path_max_tmp,
		                   playlist.
				   songs[playlist.order[playlist.queued]]));
		if (queueSong(playlist.songs[playlist.order[playlist.queued]]) <
		    0) {
			playlist.queued = -1;
			playlist_queueError = 1;
		}
	} else if (playlist.length && playlist.repeat) {
		if (playlist.length > 1 && playlist.random) {
			randomizeOrder(0, playlist.length - 1);
		}
		playlist.queued = 0;
		DEBUG("playlist: queue song %i:\"%s\"\n",
		      playlist.queued,
		      get_song_url(path_max_tmp,
		                   playlist.
		                   songs[playlist.order[playlist.queued]]));
		if (queueSong(playlist.songs[playlist.order[playlist.queued]]) <
		    0) {
			playlist.queued = -1;
			playlist_queueError = 1;
		}
	}
}

static void syncPlaylistWithQueue(int queue)
{
	if (queue && getPlayerQueueState() == PLAYER_QUEUE_BLANK) {
		queueNextSongInPlaylist();
	} else if (getPlayerQueueState() == PLAYER_QUEUE_DECODE) {
		if (playlist.queued != -1)
			setQueueState(PLAYER_QUEUE_PLAY);
		else
			setQueueState(PLAYER_QUEUE_STOP);
	} else if (getPlayerQueueState() == PLAYER_QUEUE_EMPTY) {
		setQueueState(PLAYER_QUEUE_BLANK);
		if (playlist.queued >= 0) {
			DEBUG("playlist: now playing queued song\n");
			playlist.current = playlist.queued;
		}
		playlist.queued = -1;
		if (queue)
			queueNextSongInPlaylist();
	}
}

static void lockPlaylistInteraction(void)
{
	if (getPlayerQueueState() == PLAYER_QUEUE_PLAY ||
	    getPlayerQueueState() == PLAYER_QUEUE_FULL) {
		playerQueueLock();
		syncPlaylistWithQueue(0);
	}
}

static void unlockPlaylistInteraction(void)
{
	playerQueueUnlock();
}

static void clearPlayerQueue(void)
{
	playlist.queued = -1;
	switch (getPlayerQueueState()) {
	case PLAYER_QUEUE_FULL:
		DEBUG("playlist: dequeue song\n");
		setQueueState(PLAYER_QUEUE_BLANK);
		break;
	case PLAYER_QUEUE_PLAY:
		DEBUG("playlist: stop decoding queued song\n");
		setQueueState(PLAYER_QUEUE_STOP);
		break;
	}
}

int addToPlaylist(int fd, char *url, int *added_id)
{
	Song *song;

	DEBUG("add to playlist: %s\n", url);

	if ((song = getSongFromDB(url))) {
	} else if (!(isValidRemoteUtf8Url(url) &&
		     (song = newSong(url, SONG_TYPE_URL, NULL)))) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "\"%s\" is not in the music db or is "
			     "not a valid url", url);
		return -1;
	}

	return addSongToPlaylist(fd, song, added_id);
}

int addToStoredPlaylist(int fd, char *url, char *utf8file)
{
	Song *song;

	DEBUG("add to stored playlist: %s\n", url);

	song = getSongFromDB(url);
	if (song) {
		appendSongToStoredPlaylistByPath(fd, utf8file, song);
		return 0;
	}

	if (!isValidRemoteUtf8Url(url))
		goto fail;

	song = newSong(url, SONG_TYPE_URL, NULL);
	if (song) {
		appendSongToStoredPlaylistByPath(fd, utf8file, song);
		freeJustSong(song);
		return 0;
	}

fail:
	commandError(fd, ACK_ERROR_NO_EXIST, "\"%s\" is not in the music db"
	             "or is not a valid url", url);
	return -1;
}

int addSongToPlaylist(int fd, Song * song, int *added_id)
{
	int id;

	if (playlist.length == playlist_max_length) {
		commandError(fd, ACK_ERROR_PLAYLIST_MAX,
			     "playlist is at the max size");
		return -1;
	}

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		if (playlist.queued >= 0
		    && playlist.current == playlist.length - 1) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	id = getNextId();

	playlist.songs[playlist.length] = song;
	playlist.songMod[playlist.length] = playlist.version;
	playlist.order[playlist.length] = playlist.length;
	playlist.positionToId[playlist.length] = id;
	playlist.idToPosition[playlist.positionToId[playlist.length]] =
	    playlist.length;
	playlist.length++;

	if (playlist.random) {
		int swap;
		int start;
		/*if(playlist_state==PLAYLIST_STATE_STOP) start = 0;
		   else */ if (playlist.queued >= 0)
			start = playlist.queued + 1;
		else
			start = playlist.current + 1;
		if (start < playlist.length) {
			swap = random() % (playlist.length - start);
			swap += start;
			swapOrder(playlist.length - 1, swap);
		}
	}

	incrPlaylistVersion();

	if (added_id)
		*added_id = id;

	return 0;
}

int swapSongsInPlaylist(int fd, int song1, int song2)
{
	int queuedSong = -1;
	int currentSong;

	if (song1 < 0 || song1 >= playlist.length) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "song doesn't exist: \"%i\"", song1);
		return -1;
	}
	if (song2 < 0 || song2 >= playlist.length) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "song doesn't exist: \"%i\"", song2);
		return -1;
	}

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		if (playlist.queued >= 0) {
			queuedSong = playlist.order[playlist.queued];
		}
		currentSong = playlist.order[playlist.current];

		if (queuedSong == song1 || queuedSong == song2
		    || currentSong == song1 || currentSong == song2) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	swapSongs(song1, song2);
	if (playlist.random) {
		int i;
		int k;
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
		if (playlist.current == song1)
			playlist.current = song2;
		else if (playlist.current == song2)
			playlist.current = song1;
	}

	incrPlaylistVersion();

	return 0;
}

int swapSongsInPlaylistById(int fd, int id1, int id2)
{
	checkSongId(id1);
	checkSongId(id2);

	return swapSongsInPlaylist(fd, playlist.idToPosition[id1],
				   playlist.idToPosition[id2]);
}

#define moveSongFromTo(from, to) { \
	playlist.idToPosition[playlist.positionToId[from]] = to; \
	playlist.positionToId[to] = playlist.positionToId[from]; \
	playlist.songs[to] = playlist.songs[from]; \
	playlist.songMod[to] = playlist.version; \
}

int deleteFromPlaylist(int fd, int song)
{
	int i;
	int songOrder;

	if (song < 0 || song >= playlist.length) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "song doesn't exist: \"%i\"", song);
		return -1;
	}

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		if (playlist.queued >= 0
		    && (playlist.order[playlist.queued] == song
			|| playlist.order[playlist.current] == song)) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	if (playlist.songs[song]->type == SONG_TYPE_URL) {
		freeJustSong(playlist.songs[song]);
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
	    && playlist.current == songOrder) {
		/*if(playlist.current>=playlist.length) return playerStop(fd);
		   else return playPlaylistOrderNumber(fd,playlist.current); */
		playerWait(STDERR_FILENO);
		playlist_noGoToNext = 1;
	}

	if (playlist.current > songOrder) {
		playlist.current--;
	} else if (playlist.current >= playlist.length) {
		incrPlaylistCurrent();
	}

	if (playlist.queued > songOrder) {
		playlist.queued--;
	}

	return 0;
}

int deleteFromPlaylistById(int fd, int id)
{
	checkSongId(id);

	return deleteFromPlaylist(fd, playlist.idToPosition[id]);
}

void deleteASongFromPlaylist(Song * song)
{
	int i;

	if (NULL == playlist.songs)
		return;

	for (i = 0; i < playlist.length; i++) {
		if (song == playlist.songs[i]) {
			deleteFromPlaylist(STDERR_FILENO, i);
		}
	}
}

int stopPlaylist(int fd)
{
	DEBUG("playlist: stop\n");
	if (playerWait(fd) < 0)
		return -1;
	playlist.queued = -1;
	playlist_state = PLAYLIST_STATE_STOP;
	playlist_noGoToNext = 0;
	if (playlist.random)
		randomizeOrder(0, playlist.length - 1);
	return 0;
}

static int playPlaylistOrderNumber(int fd, int orderNum)
{
	char path_max_tmp[MPD_PATH_MAX];

	if (playerStop(fd) < 0)
		return -1;

	playlist_state = PLAYLIST_STATE_PLAY;
	playlist_noGoToNext = 0;
	playlist.queued = -1;
	playlist_queueError = 0;

	DEBUG("playlist: play %i:\"%s\"\n", orderNum,
	      get_song_url(path_max_tmp,
	                   playlist.songs[playlist.order[orderNum]]));

	if (playerPlay(fd, (playlist.songs[playlist.order[orderNum]])) < 0) {
		stopPlaylist(fd);
		return -1;
	} 

	playlist.current = orderNum;

	return 0;
}

int playPlaylist(int fd, int song, int stopOnError)
{
	int i = song;

	clearPlayerError();

	if (song == -1) {
		if (playlist.length == 0)
			return 0;

		if (playlist_state == PLAYLIST_STATE_PLAY) {
			return playerSetPause(fd, 0);
		}
		if (playlist.current >= 0 && playlist.current < playlist.length) {
			i = playlist.current;
		} else {
			i = 0;
		}
	} else if (song < 0 || song >= playlist.length) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "song doesn't exist: \"%i\"", song);
		return -1;
	}

	if (playlist.random) {
		if (song == -1 && playlist_state == PLAYLIST_STATE_PLAY) {
			randomizeOrder(0, playlist.length - 1);
		} else {
			if (song >= 0)
				for (i = 0; song != playlist.order[i]; i++) ;
			if (playlist_state == PLAYLIST_STATE_STOP) {
				playlist.current = 0;
			}
			swapOrder(i, playlist.current);
			i = playlist.current;
		}
	}

	playlist_stopOnError = stopOnError;
	playlist_errorCount = 0;

	return playPlaylistOrderNumber(fd, i);
}

int playPlaylistById(int fd, int id, int stopOnError)
{
	if (id == -1) {
		return playPlaylist(fd, id, stopOnError);
	}

	checkSongId(id);

	return playPlaylist(fd, playlist.idToPosition[id], stopOnError);
}

static void syncCurrentPlayerDecodeMetadata(void)
{
	Song *songPlayer = playerCurrentDecodeSong();
	Song *song;
	int songNum;
	char path_max_tmp[MPD_PATH_MAX];

	if (!songPlayer)
		return;

	if (playlist_state != PLAYLIST_STATE_PLAY)
		return;

	songNum = playlist.order[playlist.current];
	song = playlist.songs[songNum];

	if (song->type == SONG_TYPE_URL &&
	    0 == strcmp(get_song_url(path_max_tmp, song), songPlayer->url) &&
	    !mpdTagsAreEqual(song->tag, songPlayer->tag)) {
		if (song->tag)
			freeMpdTag(song->tag);
		song->tag = mpdTagDup(songPlayer->tag);
		playlist.songMod[songNum] = playlist.version;
		incrPlaylistVersion();
	}
}

void syncPlayerAndPlaylist(void)
{
	if (playlist_state != PLAYLIST_STATE_PLAY)
		return;

	if (getPlayerState() == PLAYER_STATE_STOP)
		playPlaylistIfPlayerStopped();
	else
		syncPlaylistWithQueue(!playlist_queueError);

	syncCurrentPlayerDecodeMetadata();
}

static int currentSongInPlaylist(int fd)
{
	if (playlist_state != PLAYLIST_STATE_PLAY)
		return 0;

	playlist_stopOnError = 0;

	syncPlaylistWithQueue(0);

	if (playlist.current >= 0 && playlist.current < playlist.length) {
		return playPlaylistOrderNumber(fd, playlist.current);
	} else
		return stopPlaylist(fd);
}

int nextSongInPlaylist(int fd)
{
	if (playlist_state != PLAYLIST_STATE_PLAY)
		return 0;

	syncPlaylistWithQueue(0);

	playlist_stopOnError = 0;

	if (playlist.current < playlist.length - 1) {
		return playPlaylistOrderNumber(fd, playlist.current + 1);
	} else if (playlist.length && playlist.repeat) {
		if (playlist.random)
			randomizeOrder(0, playlist.length - 1);
		return playPlaylistOrderNumber(fd, 0);
	} else {
		incrPlaylistCurrent();
		return stopPlaylist(fd);
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
			stopPlaylist(STDERR_FILENO);
		} else if (playlist_noGoToNext)
			currentSongInPlaylist(STDERR_FILENO);
		else
			nextSongInPlaylist(STDERR_FILENO);
	}
}

int getPlaylistRepeatStatus(void)
{
	return playlist.repeat;
}

int getPlaylistRandomStatus(void)
{
	return playlist.random;
}

int setPlaylistRepeatStatus(int fd, int status)
{
	if (status != 0 && status != 1) {
		commandError(fd, ACK_ERROR_ARG, "\"%i\" is not 0 or 1", status);
		return -1;
	}

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		if (playlist.repeat && !status && playlist.queued == 0) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	playlist.repeat = status;

	return 0;
}

int moveSongInPlaylist(int fd, int from, int to)
{
	int i;
	Song *tmpSong;
	int tmpId;
	int currentSong;

	if (from < 0 || from >= playlist.length) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "song doesn't exist: \"%i\"", from);
		return -1;
	}

	if ((to >= 0 && to >= playlist.length) ||
	    (to < 0 && abs(to) > playlist.length)) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "song doesn't exist: \"%i\"", to);
		return -1;
	}

	if (from == to) /* no-op */
		return 0;

	/*
	 * (to < 0) => move to offset from current song
	 * (-playlist.length == to) => move to position BEFORE current song
	 */
	currentSong = playlist.order[playlist.current];
	if (to < 0 && playlist.current >= 0) {
		if (currentSong == from)
			/* no-op, can't be moved to offset of itself */
			return 0;
		to = (currentSong + abs(to)) % playlist.length;
	}

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		int queuedSong = -1;

		if (playlist.queued >= 0)
			queuedSong = playlist.order[playlist.queued];
		if (queuedSong == from || queuedSong == to
		    || currentSong == from || currentSong == to) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	tmpSong = playlist.songs[from];
	tmpId = playlist.positionToId[from];
	/* move songs to one less in from->to */
	for (i = from; i < to; i++) {
		moveSongFromTo(i + 1, i);
	}
	/* move songs to one more in to->from */
	for (i = from; i > to; i--) {
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
			if (playlist.order[i] > from && playlist.order[i] <= to) {
				playlist.order[i]--;
			} else if (playlist.order[i] < from &&
				   playlist.order[i] >= to) {
				playlist.order[i]++;
			} else if (from == playlist.order[i]) {
				playlist.order[i] = to;
			}
		}
	}
	else
	{
		if (playlist.current == from)
			playlist.current = to;
		else if (playlist.current > from && playlist.current <= to) {
			playlist.current--;
		} else if (playlist.current >= to && playlist.current < from) {
			playlist.current++;
		}

		/* this first if statement isn't necessary since the queue
		 * would have been cleared out if queued == from */
		if (playlist.queued == from)
			playlist.queued = to;
		else if (playlist.queued > from && playlist.queued <= to) {
			playlist.queued--;
		} else if (playlist.queued>= to && playlist.queued < from) {
			playlist.queued++;
		}
	}

	incrPlaylistVersion();

	return 0;
}

int moveSongInPlaylistById(int fd, int id1, int to)
{
	checkSongId(id1);

	return moveSongInPlaylist(fd, playlist.idToPosition[id1], to);
}

static void orderPlaylist(void)
{
	int i;

	if (playlist.current >= 0 && playlist.current < playlist.length) {
		playlist.current = playlist.order[playlist.current];
	}

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		if (playlist.queued >= 0) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

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

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		if (playlist.queued >= start && playlist.queued <= end) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	for (i = start; i <= end; i++) {
		ri = random() % (end - start + 1) + start;
		if (ri == playlist.current)
			playlist.current = i;
		else if (i == playlist.current)
			playlist.current = ri;
		swapOrder(i, ri);
	}
}

int setPlaylistRandomStatus(int fd, int status)
{
	int statusWas = playlist.random;

	if (status != 0 && status != 1) {
		commandError(fd, ACK_ERROR_ARG, "\"%i\" is not 0 or 1", status);
		return -1;
	}

	playlist.random = status;

	if (status != statusWas) {
		if (playlist.random) {
			/*if(playlist_state==PLAYLIST_STATE_PLAY) {
			   randomizeOrder(playlist.current+1,
			   playlist.length-1);
			   }
			   else */ randomizeOrder(0, playlist.length - 1);
			if (playlist.current >= 0 &&
			    playlist.current < playlist.length) {
				swapOrder(playlist.current, 0);
				playlist.current = 0;
			}
		} else
			orderPlaylist();
	}

	return 0;
}

int previousSongInPlaylist(int fd)
{
	static time_t lastTime;
	time_t diff = time(NULL) - lastTime;

	lastTime += diff;

	if (playlist_state != PLAYLIST_STATE_PLAY)
		return 0;

	syncPlaylistWithQueue(0);

	if (diff && getPlayerElapsedTime() > PLAYLIST_PREV_UNLESS_ELAPSED) {
		return playPlaylistOrderNumber(fd, playlist.current);
	} else {
		if (playlist.current > 0) {
			return playPlaylistOrderNumber(fd,
						       playlist.current - 1);
		} else if (playlist.repeat) {
			return playPlaylistOrderNumber(fd, playlist.length - 1);
		} else {
			return playPlaylistOrderNumber(fd, playlist.current);
		}
	}
}

int shufflePlaylist(int fd)
{
	int i;
	int ri;

	if (playlist.length > 1) {
		if (playlist_state == PLAYLIST_STATE_PLAY) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
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
			ri = random() % (playlist.length - 1) + 1;
			swapSongs(i, ri);
		}

		incrPlaylistVersion();
	}

	return 0;
}

int deletePlaylist(int fd, char *utf8file)
{
	char path_max_tmp[MPD_PATH_MAX];

	utf8_to_fs_playlist_path(path_max_tmp, utf8file);

	if (!isPlaylist(path_max_tmp)) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "playlist \"%s\" not found", utf8file);
		return -1;
	}

	if (unlink(path_max_tmp) < 0) {
		commandError(fd, ACK_ERROR_SYSTEM,
			     "problems deleting file");
		return -1;
	}

	return 0;
}

int savePlaylist(int fd, char *utf8file)
{
	FILE *fp;
	int i;
	struct stat sb;
	char path_max_tmp[MPD_PATH_MAX];

	if (!valid_playlist_name(fd, utf8file))
		return -1;

	utf8_to_fs_playlist_path(path_max_tmp, utf8file);
	if (!stat(path_max_tmp, &sb)) {
		commandError(fd, ACK_ERROR_EXIST, "a file or directory already "
			     "exists with the name \"%s\"", utf8file);
		return -1;
	}

	while (!(fp = fopen(path_max_tmp, "w")) && errno == EINTR);

	if (fp == NULL) {
		commandError(fd, ACK_ERROR_SYSTEM, "failed to create file");
		return -1;
	}

	for (i = 0; i < playlist.length; i++) {
		char tmp[MPD_PATH_MAX];

		get_song_url(path_max_tmp, playlist.songs[i]);
		utf8_to_fs_charset(tmp, path_max_tmp);

		if (playlist_saveAbsolutePaths &&
		    playlist.songs[i]->type == SONG_TYPE_FILE)
			fprintf(fp, "%s\n", rmp2amp_r(tmp, tmp));
		else
			fprintf(fp, "%s\n", tmp);
	}

	while (fclose(fp) && errno == EINTR) ;

	return 0;
}

int getPlaylistCurrentSong(void)
{
	if (playlist.current >= 0 && playlist.current < playlist.length) {
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

int seekSongInPlaylist(int fd, int song, float seek_time)
{
	int i = song;

	if (song < 0 || song >= playlist.length) {
		commandError(fd, ACK_ERROR_NO_EXIST,
			     "song doesn't exist: \"%i\"", song);
		return -1;
	}

	if (playlist.random)
		for (i = 0; song != playlist.order[i]; i++) ;

	clearPlayerError();
	playlist_stopOnError = 1;
	playlist_errorCount = 0;

	if (playlist_state == PLAYLIST_STATE_PLAY) {
		if (playlist.queued >= 0) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	} else if (playPlaylistOrderNumber(fd, i) < 0)
		return -1;

	if (playlist.current != i) {
		if (playPlaylistOrderNumber(fd, i) < 0)
			return -1;
	}

	return playerSeek(fd, playlist.songs[playlist.order[i]], seek_time);
}

int seekSongInPlaylistById(int fd, int id, float seek_time)
{
	checkSongId(id);

	return seekSongInPlaylist(fd, playlist.idToPosition[id], seek_time);
}

int getPlaylistSongId(int song)
{
	return playlist.positionToId[song];
}

int PlaylistInfo(int fd, char *utf8file, int detail)
{
	ListNode *node;
	List *list;

	if (!(list = loadStoredPlaylist(fd, utf8file)))
		return -1;

	node = list->firstNode;
	while (node != NULL) {
		char *temp = node->data;
		int wrote = 0;

		if (detail) {
			Song *song = getSongFromDB(temp);
			if (song) {
				printSongInfo(fd, song);
				wrote = 1;
			}
		}

		if (!wrote) {
			fdprintf(fd, SONG_FILE "%s\n", temp);
		}

		node = node->nextNode;
	}

	freeList(list);
	return 0;
}

int loadPlaylist(int fd, char *utf8file)
{
	ListNode *node;
	List *list;

	if (!(list = loadStoredPlaylist(fd,  utf8file)))
		return -1;

	node = list->firstNode;
	while (node != NULL) {
		char *temp = node->data;
		if ((addToPlaylist(STDERR_FILENO, temp, NULL)) < 0) {
			/* for windows compatibility, convert slashes */
			char *temp2 = xstrdup(temp);
			char *p = temp2;
			while (*p) {
				if (*p == '\\')
					*p = '/';
				p++;
			}
			if ((addToPlaylist(STDERR_FILENO, temp2, NULL)) < 0) {
				commandError(fd, ACK_ERROR_PLAYLIST_LOAD,
							"can't add file \"%s\"", temp2);
			}
			free(temp2);
		}

		node = node->nextNode;
	}

	freeList(list);
	return 0;
}

void searchForSongsInPlaylist(int fd, int numItems, LocateTagItem * items)
{
	int i;
	char **originalNeedles = xmalloc(numItems * sizeof(char *));

	for (i = 0; i < numItems; i++) {
		originalNeedles[i] = items[i].needle;
		items[i].needle = strDupToUpper(originalNeedles[i]);
	}

	for (i = 0; i < playlist.length; i++) {
		if (strstrSearchTags(playlist.songs[i], numItems, items))
			printPlaylistSongInfo(fd, i);
	}

	for (i = 0; i < numItems; i++) {
		free(items[i].needle);
		items[i].needle = originalNeedles[i];
	}

	free(originalNeedles);
}

void findSongsInPlaylist(int fd, int numItems, LocateTagItem * items)
{
	int i;

	for (i = 0; i < playlist.length; i++) {
		if (tagItemsFoundAndMatches(playlist.songs[i], numItems, items))
			printPlaylistSongInfo(fd, i);
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
int valid_playlist_name(int err_fd, const char *utf8path)
{
	if (strchr(utf8path, '/') ||
	    strchr(utf8path, '\n') ||
	    strchr(utf8path, '\r')) {
		commandError(err_fd, ACK_ERROR_ARG, "playlist name \"%s\" is "
		             "invalid: playlist names may not contain slashes,"
			     " newlines or carriage returns",
		             utf8path);
		return 0;
	}
	return 1;
}


