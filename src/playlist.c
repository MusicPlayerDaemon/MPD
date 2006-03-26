/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#define PLAYLIST_COMMENT	'#'

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

#define PLAYLIST_BUFFER_SIZE	2*MAXPATHLEN

#define PLAYLIST_HASH_MULT	4

#define DEFAULT_PLAYLIST_MAX_LENGTH		(1024*16)
#define DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS	0

typedef struct _Playlist {
	Song ** songs;
	/* holds version a song was modified on */
	mpd_uint32 * songMod;
	int * order;
	int * positionToId;
	int * idToPosition;
	int length;
	int current;
	int queued;
	int repeat;
	int random;
	mpd_uint32 version;
} Playlist;

static Playlist playlist;
static int playlist_state = PLAYLIST_STATE_STOP;
static int playlist_max_length = DEFAULT_PLAYLIST_MAX_LENGTH;
static int playlist_stopOnError;
static int playlist_errorCount = 0;
static int playlist_queueError;
static int playlist_noGoToNext = 0;

static int playlist_saveAbsolutePaths = DEFAULT_PLAYLIST_SAVE_ABSOLUTE_PATHS;

static void swapOrder(int a, int b);
static int playPlaylistOrderNumber(FILE * fp, int orderNum);
static void randomizeOrder(int start, int end);

static char * getStateFile() {
	ConfigParam * param = parseConfigFilePath(CONF_STATE_FILE, 0);
	
	if(!param) return NULL;

	return param->value;
}

static void incrPlaylistVersion() {
	static unsigned long max = ((mpd_uint32)1<<31)-1;
	playlist.version++;
	if(playlist.version>=max) {
		int i;

		for(i=0; i<playlist.length; i++) {
			playlist.songMod[i] = 0;
		}

		playlist.version = 1;
	}
}

void playlistVersionChange() {
	int i = 0;

	for(i=0; i<playlist.length; i++) {
		playlist.songMod[i] = playlist.version;
	}

	incrPlaylistVersion();
}

static void incrPlaylistCurrent() {
	if(playlist.current < 0) return;

	if(playlist.current >= playlist.length-1) {
		if(playlist.repeat) playlist.current = 0;
		else playlist.current = -1;
	}
	else playlist.current++;
}

void initPlaylist() {
	char * test;
	int i;
	ConfigParam * param;

	playlist.length = 0;
	playlist.repeat = 0;
	playlist.version = 1;
	playlist.random = 0;
	playlist.queued = -1;
        playlist.current = -1;

	param = getConfigParam(CONF_MAX_PLAYLIST_LENGTH);

	if(param) {
		playlist_max_length = strtol(param->value, &test, 10);
		if(*test!='\0') {
			ERROR("max playlist length \"%s\" is not an integer, "
					"line %i\n", param->value, param->line);
			exit(EXIT_FAILURE);
		}
	}

	param = getConfigParam(CONF_SAVE_ABSOLUTE_PATHS);

	if(param) {
		if(0 == strcmp("yes", param->value) ) {
			playlist_saveAbsolutePaths = 1;
		}
		else if(0 == strcmp("no", param->value) ) {
			playlist_saveAbsolutePaths = 0;
		}
		else {
			ERROR("%s \"%s\" is not yes or no, line %i"
				CONF_SAVE_ABSOLUTE_PATHS,
				param->value, param->line);
			exit(EXIT_FAILURE);
		}
	}

	playlist.songs = malloc(sizeof(Song *)*playlist_max_length);
	playlist.songMod = malloc(sizeof(mpd_uint32)*playlist_max_length);
	playlist.order = malloc(sizeof(int)*playlist_max_length);
	playlist.idToPosition = malloc(sizeof(int)*playlist_max_length*
					PLAYLIST_HASH_MULT);
	playlist.positionToId = malloc(sizeof(int)*playlist_max_length);

	memset(playlist.songs,0,sizeof(char *)*playlist_max_length);

	srandom(time(NULL));

	for(i=0; i<playlist_max_length*PLAYLIST_HASH_MULT; i++) {
		playlist.idToPosition[i] = -1;
	}
}

static int getNextId() {
	static int cur = 0;

	while(playlist.idToPosition[cur] != -1) {
		cur++;
		if(cur >= playlist_max_length*PLAYLIST_HASH_MULT) {
			cur = 0;
		}
	}

	return cur;
}

void finishPlaylist() {
        int i;
        for(i=0;i<playlist.length;i++) {
		if(playlist.songs[i]->type == SONG_TYPE_URL) {
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

int clearPlaylist(FILE * fp) {
	int i;

	if(stopPlaylist(fp)<0) return -1;

	for(i=0;i<playlist.length;i++) {
		if(playlist.songs[i]->type == SONG_TYPE_URL) {
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

int showPlaylist(FILE * fp) {
	int i;

	for(i=0;i<playlist.length;i++) {
		myfprintf(fp,"%i:%s\n", i, getSongUrl(playlist.songs[i]));
	}

	return 0;
}

void savePlaylistState() {
	char * stateFile = getStateFile();
	
	if(stateFile) {
		FILE * fp;

		while(!(fp = fopen(stateFile,"w")) && errno==EINTR);
		if(!fp) {
			ERROR("problems opening state file \"%s\" for "
				"writing: %s\n", stateFile,
				strerror(errno));
			return;
		}

		myfprintf(fp,"%s",PLAYLIST_STATE_FILE_STATE);
		switch(playlist_state) {
		case PLAYLIST_STATE_PLAY:
			switch(getPlayerState()) {
			case PLAYER_STATE_PAUSE:
				myfprintf(fp,"%s\n",
					PLAYLIST_STATE_FILE_STATE_PAUSE);
				break;
			default:
				myfprintf(fp,"%s\n",
					PLAYLIST_STATE_FILE_STATE_PLAY);
			}
			myfprintf(fp,"%s%i\n",PLAYLIST_STATE_FILE_CURRENT,
				playlist.order[playlist.current]);
			myfprintf(fp,"%s%i\n",PLAYLIST_STATE_FILE_TIME,
				getPlayerElapsedTime());
			break;
		default:
			myfprintf(fp,"%s\n",PLAYLIST_STATE_FILE_STATE_STOP);
			break;
		}
		myfprintf(fp,"%s%i\n",PLAYLIST_STATE_FILE_RANDOM,
				playlist.random);
		myfprintf(fp,"%s%i\n",PLAYLIST_STATE_FILE_REPEAT,
				playlist.repeat);
		myfprintf(fp,"%s%i\n",PLAYLIST_STATE_FILE_CROSSFADE,
				(int)(getPlayerCrossFade()));
		myfprintf(fp,"%s\n",PLAYLIST_STATE_FILE_PLAYLIST_BEGIN);
		showPlaylist(fp);
		myfprintf(fp,"%s\n",PLAYLIST_STATE_FILE_PLAYLIST_END);

		while(fclose(fp) && errno==EINTR);
	}
}

void loadPlaylistFromStateFile(FILE * fp, char * buffer, int state, int current,
		int time) 
{
	char * temp;
	int song;
	char * stateFile = getStateFile();

	if(!myFgets(buffer,PLAYLIST_BUFFER_SIZE,fp)) {
		ERROR("error parsing state file \"%s\"\n", stateFile);
		exit(EXIT_FAILURE);
	}
	while(strcmp(buffer,PLAYLIST_STATE_FILE_PLAYLIST_END)) {
		song = atoi(strtok(buffer,":"));
		if(!(temp = strtok(NULL,""))) {
			ERROR("error parsing state file \"%s\"\n", stateFile);
			exit(EXIT_FAILURE);
		}
		if(addToPlaylist(stderr, temp, 0)==0 && current==song) {
			if(state!=PLAYER_STATE_STOP) {
				playPlaylist(stderr,playlist.length-1,0);
			}
			if(state==PLAYER_STATE_PAUSE) {
				playerPause(stderr);
			}
			if(state!=PLAYER_STATE_STOP) {
				seekSongInPlaylist(stderr,playlist.length-1,
						time);
			}
		}
		if(!myFgets(buffer,PLAYLIST_BUFFER_SIZE,fp)) {
			ERROR("error parsing state file \"%s\"\n", stateFile);
			exit(EXIT_FAILURE);
		}
	}
}

void readPlaylistState() {
	char * stateFile = getStateFile();
	
	if(stateFile) {
		FILE * fp;
		struct stat st;
		int current = -1;
		int time = 0;
		int state = PLAYER_STATE_STOP;
		char buffer[PLAYLIST_BUFFER_SIZE];

		if(stat(stateFile,&st)<0) {
			DEBUG("failed to stat state file\n");
			return;
		}
		if(!S_ISREG(st.st_mode)) {
			ERROR("state file \"%s\" is not a regular "
				"file\n",stateFile);
			exit(EXIT_FAILURE);
		}

		fp = fopen(stateFile,"r");
		if(!fp) {
			ERROR("problems opening state file \"%s\" for "
				"reading: %s\n", stateFile,
				strerror(errno));
			exit(EXIT_FAILURE);
		}

		while(myFgets(buffer,PLAYLIST_BUFFER_SIZE,fp)) {
			if(strncmp(buffer,PLAYLIST_STATE_FILE_STATE,
				strlen(PLAYLIST_STATE_FILE_STATE))==0) {
				if(strcmp(&(buffer
					[strlen(PLAYLIST_STATE_FILE_STATE)]),
					PLAYLIST_STATE_FILE_STATE_PLAY)==0) {
					state = PLAYER_STATE_PLAY;
				}
				else if(strcmp(&(buffer
					[strlen(PLAYLIST_STATE_FILE_STATE)]),
					PLAYLIST_STATE_FILE_STATE_PAUSE)==0) {
					state = PLAYER_STATE_PAUSE;
				}
			}
			else if(strncmp(buffer,PLAYLIST_STATE_FILE_TIME,
				strlen(PLAYLIST_STATE_FILE_TIME))==0) {
				time = atoi(&(buffer
					[strlen(PLAYLIST_STATE_FILE_TIME)]));
			}
			else if(strncmp(buffer,PLAYLIST_STATE_FILE_REPEAT,
				strlen(PLAYLIST_STATE_FILE_REPEAT))==0) {
				if(strcmp(&(buffer
					[strlen(PLAYLIST_STATE_FILE_REPEAT)]),
					"1")==0) {
					setPlaylistRepeatStatus(stderr,1);
				}
				else setPlaylistRepeatStatus(stderr,0);
			}
			else if(strncmp(buffer,PLAYLIST_STATE_FILE_CROSSFADE,
				strlen(PLAYLIST_STATE_FILE_CROSSFADE))==0) {
				setPlayerCrossFade(atoi(&(buffer[strlen(
                                       	PLAYLIST_STATE_FILE_CROSSFADE)])));
			}
			else if(strncmp(buffer,PLAYLIST_STATE_FILE_RANDOM,
				strlen(PLAYLIST_STATE_FILE_RANDOM))==0) {
				if(strcmp(&(buffer
					[strlen(PLAYLIST_STATE_FILE_RANDOM)]),
					"1")==0) {
					setPlaylistRandomStatus(stderr,1);
				}
				else setPlaylistRandomStatus(stderr,0);
			}
			else if(strncmp(buffer,PLAYLIST_STATE_FILE_CURRENT,
				strlen(PLAYLIST_STATE_FILE_CURRENT))==0) {
				if(strlen(buffer)==
					strlen(PLAYLIST_STATE_FILE_CURRENT)) {
					ERROR("error parsing state "
						"file \"%s\"\n",
						stateFile);
					exit(EXIT_FAILURE);
				}
				current = atoi(&(buffer
					[strlen(PLAYLIST_STATE_FILE_CURRENT)]));
			}
			else if(strncmp(buffer,
				PLAYLIST_STATE_FILE_PLAYLIST_BEGIN,
				strlen(PLAYLIST_STATE_FILE_PLAYLIST_BEGIN)
				)==0) {
				if(state==PLAYER_STATE_STOP) current = -1;
				loadPlaylistFromStateFile(fp,buffer,state,
						current,time);
			}
		}

		fclose(fp);
	}
}

void printPlaylistSongInfo(FILE * fp, int song) {
	printSongInfo(fp, playlist.songs[song]);
	myfprintf(fp, "Pos: %i\n", song);
	myfprintf(fp, "Id: %i\n", playlist.positionToId[song]);
}

int playlistChanges(FILE * fp, mpd_uint32 version) {
	int i;
	
	for(i=0; i<playlist.length; i++) {
		if(version > playlist.version ||
				playlist.songMod[i] >= version ||
				playlist.songMod[i] == 0)
		{
			printPlaylistSongInfo(fp, i);
		}
	}

	return 0;
}

int playlistInfo(FILE * fp, int song) {
	int i;
	int begin = 0;
	int end = playlist.length;

	if(song>=0) {
		begin = song;
		end = song+1;
	}
	if(song>=playlist.length) {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "song doesn't exist: \"%i\"", song);
		return -1;
	}

	for(i=begin; i<end; i++) printPlaylistSongInfo(fp, i);

	return 0;
}

# define checkSongId(id) { \
	if(id < 0 || id >= PLAYLIST_HASH_MULT*playlist_max_length || \
			playlist.idToPosition[id] == -1 ) \
	{ \
		commandError(fp, ACK_ERROR_NO_EXIST, \
                                "song id doesn't exist: \"%i\"", id); \
		return -1; \
	} \
}

int playlistId(FILE * fp, int id) {
	int i;
	int begin = 0;
	int end = playlist.length;

	if(id>=0) {
		checkSongId(id);
		begin = playlist.idToPosition[id];
		end = begin+1;
	}

	for(i=begin; i<end; i++) printPlaylistSongInfo(fp, i);

	return 0;
}

void swapSongs(int song1, int song2) {
	Song * sTemp;
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

void queueNextSongInPlaylist() {
	if(playlist.current<playlist.length-1) {
		playlist.queued = playlist.current+1;
		DEBUG("playlist: queue song %i:\"%s\"\n",
				playlist.queued,
				getSongUrl(playlist.songs[playlist.order[
					playlist.queued]]));
		if(queueSong(playlist.songs[playlist.order[playlist.queued]]) <
                                0) 
                {
			playlist.queued = -1;
			playlist_queueError = 1;
		}
	}
	else if(playlist.length && playlist.repeat) {
		if(playlist.length>1 && playlist.random) {
			randomizeOrder(0,playlist.length-1);
		}
		playlist.queued = 0;
		DEBUG("playlist: queue song %i:\"%s\"\n",
				playlist.queued,
				getSongUrl(playlist.songs[playlist.order[
					playlist.queued]]));
		if(queueSong(playlist.songs[playlist.order[playlist.queued]]) <
				0) 
                {
			playlist.queued = -1;
			playlist_queueError = 1;
		}
	}
}

void syncPlaylistWithQueue(int queue) {
	if(queue && getPlayerQueueState()==PLAYER_QUEUE_BLANK) {
		queueNextSongInPlaylist();
	}
	else if(getPlayerQueueState()==PLAYER_QUEUE_DECODE) {
		if(playlist.queued!=-1) setQueueState(PLAYER_QUEUE_PLAY);
		else setQueueState(PLAYER_QUEUE_STOP);
	}
	else if(getPlayerQueueState()==PLAYER_QUEUE_EMPTY) {
		setQueueState(PLAYER_QUEUE_BLANK);
		if(playlist.queued>=0) {
			DEBUG("playlist: now playing queued song\n");
			playlist.current = playlist.queued;
		}
		playlist.queued = -1;
		if(queue) queueNextSongInPlaylist();
	}
}

void lockPlaylistInteraction() {
	if(getPlayerQueueState()==PLAYER_QUEUE_PLAY || 
		getPlayerQueueState()==PLAYER_QUEUE_FULL) {
		playerQueueLock();
		syncPlaylistWithQueue(0);
	}
}

void unlockPlaylistInteraction() {
	playerQueueUnlock();
}

void clearPlayerQueue() {
	playlist.queued = -1;
	switch(getPlayerQueueState()) {
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

int addToPlaylist(FILE * fp, char * url, int printId) {
	Song * song;

	DEBUG("add to playlist: %s\n",url);
	
	if((song = getSongFromDB(url))) {
	}
	else if(isValidRemoteUtf8Url(url) && 
                        (song = newSong(url, SONG_TYPE_URL, NULL))) 
        {
	}
	else {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "\"%s\" is not in the music db or is "
                                "not a valid url\n", url);
		return -1;
	}

	return addSongToPlaylist(fp,song, printId);
}

int addSongToPlaylist(FILE * fp, Song * song, int printId) {
	int id;

	if(playlist.length==playlist_max_length) {
		commandError(fp, ACK_ERROR_PLAYLIST_MAX,
                                "playlist is at the max size", NULL);
		return -1;
	}

	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=0 && playlist.current==playlist.length-1) {
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
	playlist.idToPosition[playlist.positionToId[playlist.length]] = playlist.length;
	playlist.length++;

	if(playlist.random) {
		int swap;
		int start;
		/*if(playlist_state==PLAYLIST_STATE_STOP) start = 0;
		else */if(playlist.queued>=0) start = playlist.queued+1;
		else start = playlist.current+1;
                if(start < playlist.length) {
		        swap = random()%(playlist.length-start);
		        swap+=start;
		        swapOrder(playlist.length-1,swap);
                }
	}
	
	incrPlaylistVersion();

	if(printId) myfprintf(fp, "Id: %i\n", id);

	return 0;
}

int swapSongsInPlaylist(FILE * fp, int song1, int song2) {
	int queuedSong = -1;
	int currentSong = -1;

	if(song1<0 || song1>=playlist.length) {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "song doesn't exist: \"%i\"", song1);
		return -1;
	}
	if(song2<0 || song2>=playlist.length) {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "song doesn't exist: \"%i\"", song2);
		return -1;
	}
	
	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=0) {
			queuedSong = playlist.order[playlist.queued];
		}
		currentSong = playlist.order[playlist.current];

		if(queuedSong==song1 || queuedSong==song2 || currentSong==song1 
				|| currentSong==song2)	
		{
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	swapSongs(song1,song2);
	if(playlist.random) {
		int i;
		int k;
		int j = -1;
		for(i=0;playlist.order[i]!=song1;i++) {
			if(playlist.order[i]==song2) j = i;
		}
		k = i;
		for(;j==-1;i++) if(playlist.order[i]==song2) j = i;
		swapOrder(k,j);
	}
	else {
		if(playlist.current==song1) playlist.current = song2;
		else if(playlist.current==song2) playlist.current = song1;
	}

	incrPlaylistVersion();

	return 0;
}

int swapSongsInPlaylistById(FILE * fp, int id1, int id2) {
	checkSongId(id1);
	checkSongId(id2);

	return swapSongsInPlaylist(fp, playlist.idToPosition[id1], 
					playlist.idToPosition[id2]);
}

#define moveSongFromTo(from, to) { \
	playlist.idToPosition[playlist.positionToId[from]] = to; \
	playlist.positionToId[to] = playlist.positionToId[from]; \
	playlist.songs[to] = playlist.songs[from]; \
	playlist.songMod[to] = playlist.version; \
}

int deleteFromPlaylist(FILE * fp, int song) {
	int i;
	int songOrder;

	if(song<0 || song>=playlist.length) {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "song doesn't exist: \"%i\"", song);
		return -1;
	}

	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=0 && (playlist.order[playlist.queued]==song
			|| playlist.order[playlist.current]==song)) 
		{
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();	
		}
	}

	if(playlist.songs[song]->type == SONG_TYPE_URL) {
		freeJustSong(playlist.songs[song]);
	}

	playlist.idToPosition[playlist.positionToId[song]] = -1;

	/* delete song from songs array */
	for(i=song;i<playlist.length-1;i++) {
		moveSongFromTo(i+1, i);
	}
	/* now find it in the order array */
	for(i=0;i<playlist.length-1;i++) {
		if(playlist.order[i]==song) break;
	}
	songOrder = i;
	/* delete the entry from the order array */
	for(;i<playlist.length-1;i++) playlist.order[i] = playlist.order[i+1];
	/* readjust values in the order array */
	for(i=0;i<playlist.length-1;i++) {
		if(playlist.order[i]>song) playlist.order[i]--;
	}
	/* now take care of other misc stuff */
	playlist.songs[playlist.length-1] = NULL;
	playlist.length--;

	incrPlaylistVersion();

	if(playlist_state!=PLAYLIST_STATE_STOP && playlist.current==songOrder) {
		/*if(playlist.current>=playlist.length) return playerStop(fp);
		else return playPlaylistOrderNumber(fp,playlist.current);*/
		playerStop(stderr);
		playlist_noGoToNext = 1;
	}

	if(playlist.current>songOrder) {
		playlist.current--;
	}
	else if(playlist.current>=playlist.length) {
		incrPlaylistCurrent();
	}

	if(playlist.queued>songOrder) {
		playlist.queued--;
	}

	return 0;
}

int deleteFromPlaylistById(FILE * fp, int id) {
	checkSongId(id);

	return deleteFromPlaylist(fp, playlist.idToPosition[id]);
}

void deleteASongFromPlaylist(Song * song) {
	int i;

	if(NULL==playlist.songs) return;
	
	for(i=0;i<playlist.length;i++) {
		if(song==playlist.songs[i]) {
			deleteFromPlaylist(stderr,i);
		}
	}
}

int stopPlaylist(FILE * fp) {
	DEBUG("playlist: stop\n");
	if(playerStop(fp)<0) return -1;
	playerCloseAudio();
	playlist.queued = -1;
	playlist_state = PLAYLIST_STATE_STOP;
	playlist_noGoToNext = 0;
	if(playlist.random) randomizeOrder(0,playlist.length-1);
	return 0;
}

int playPlaylistOrderNumber(FILE * fp, int orderNum) {

	if(playerStop(fp)<0) return -1;

	playlist_state = PLAYLIST_STATE_PLAY;
	playlist_noGoToNext = 0;
	playlist.queued = -1;
	playlist_queueError = 0;

	DEBUG("playlist: play %i:\"%s\"\n",orderNum,
			getSongUrl(playlist.songs[playlist.order[orderNum]]));

	if(playerPlay(fp,(playlist.songs[playlist.order[orderNum]])) < 0) {
		stopPlaylist(fp);
		return -1;
	}
	else playlist.current++;

	playlist.current = orderNum;

	return 0;
}

int playPlaylist(FILE * fp, int song, int stopOnError) {
	int i = song;

	clearPlayerError();

	if(song==-1) {
		if(playlist.length == 0) return 0;

                if(playlist_state == PLAYLIST_STATE_PLAY) {
                        return playerSetPause(fp, 0);
                }
                if(playlist.current >= 0 && playlist.current < playlist.length)
                {
                        i = playlist.current;
                }
                else {
                        i = 0;
                }
        }
	else if(song<0 || song>=playlist.length) {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "song doesn't exist: \"%i\"", song);
		return -1;
	}

	if(playlist.random) {
		if(song == -1 && playlist_state==PLAYLIST_STATE_PLAY) {
			randomizeOrder(0,playlist.length-1);
		}
		else {
			if(song>=0) for(i=0;song!=playlist.order[i];i++);
			if(playlist_state==PLAYLIST_STATE_STOP) {
				playlist.current = 0;
			}
			swapOrder(i,playlist.current);
			i = playlist.current;
		}
	}

	playlist_stopOnError = stopOnError;
	playlist_errorCount = 0;

	return playPlaylistOrderNumber(fp,i);
}

int playPlaylistById(FILE * fp, int id, int stopOnError) {
	if(id == -1) {
		return playPlaylist(fp, id, stopOnError);
	}

	checkSongId(id);

	return playPlaylist(fp, playlist.idToPosition[id], stopOnError);
}

void syncCurrentPlayerDecodeMetadata() {
        Song * songPlayer = playerCurrentDecodeSong();
        Song * song;
	int songNum;

        if(!songPlayer) return;

	if(playlist_state!=PLAYLIST_STATE_PLAY) return;

	songNum = playlist.order[playlist.current];
        song = playlist.songs[songNum];

        if(song->type == SONG_TYPE_URL &&
                        0 == strcmp(getSongUrl(song), songPlayer->url) &&
                        !mpdTagsAreEqual(song->tag, songPlayer->tag))
        {
                if(song->tag) freeMpdTag(song->tag);
                song->tag = mpdTagDup(songPlayer->tag);
		playlist.songMod[songNum] = playlist.version;
                incrPlaylistVersion();
        }
}

void syncPlayerAndPlaylist() {
	if(playlist_state!=PLAYLIST_STATE_PLAY) return;

	if(getPlayerState()==PLAYER_STATE_STOP) playPlaylistIfPlayerStopped();
	else syncPlaylistWithQueue(!playlist_queueError);

        syncCurrentPlayerDecodeMetadata();
}

int currentSongInPlaylist(FILE * fp) {
	if(playlist_state!=PLAYLIST_STATE_PLAY) return 0;

	playlist_stopOnError = 0;

	syncPlaylistWithQueue(0);

	if(playlist.current>= 0 && playlist.current<playlist.length) {
		return playPlaylistOrderNumber(fp,playlist.current);
	}
	else return stopPlaylist(fp);;

	return 0;
}

int nextSongInPlaylist(FILE * fp) {
	if(playlist_state!=PLAYLIST_STATE_PLAY) return 0;

	syncPlaylistWithQueue(0);
	
	playlist_stopOnError = 0;

	if(playlist.current<playlist.length-1) {
		return playPlaylistOrderNumber(fp,playlist.current+1);
	}
	else if(playlist.length && playlist.repeat) {
		if(playlist.random) randomizeOrder(0,playlist.length-1);
		return playPlaylistOrderNumber(fp,0);
	}
	else {
                incrPlaylistCurrent();
		return stopPlaylist(fp);;
	}

	return 0;
}

void playPlaylistIfPlayerStopped() {
	if(getPlayerState()==PLAYER_STATE_STOP) {
		int error = getPlayerError();

		if(error==PLAYER_ERROR_NOERROR) playlist_errorCount = 0;
		else playlist_errorCount++;

		if(playlist_state==PLAYLIST_STATE_PLAY && (
				(playlist_stopOnError && 
				error!=PLAYER_ERROR_NOERROR) ||
				error==PLAYER_ERROR_AUDIO ||
				error==PLAYER_ERROR_SYSTEM ||
				playlist_errorCount>=playlist.length)) {
			stopPlaylist(stderr);
		}
		else if(playlist_noGoToNext) currentSongInPlaylist(stderr);
		else nextSongInPlaylist(stderr);
	}
}

int getPlaylistRepeatStatus() {
	return playlist.repeat;
}

int getPlaylistRandomStatus() {
	return playlist.random;
}

int setPlaylistRepeatStatus(FILE * fp, int status) {
	if(status!=0 && status!=1) {
		commandError(fp, ACK_ERROR_ARG, "\"%i\" is not 0 or 1", status);
		return -1;
	}

	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.repeat && !status && playlist.queued==0) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	playlist.repeat = status;

	return 0;
}

int moveSongInPlaylist(FILE * fp, int from, int to) {
	int i;
	Song * tmpSong;
	int tmpId;
	int queuedSong = -1;
	int currentSong = -1;

	if(from<0 || from>=playlist.length) {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "song doesn't exist: \"%i\"", from);
		return -1;
	}

	if(to<0 || to>=playlist.length) {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "song doesn't exist: \"%i\"", to);
		return -1;
	}

	
	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=0) {
			queuedSong = playlist.order[playlist.queued];
		}
		currentSong = playlist.order[playlist.current];
		if(queuedSong==from || queuedSong==to || currentSong==from ||
				currentSong==to)
		{
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	tmpSong = playlist.songs[from];
	tmpId = playlist.positionToId[from];
	/* move songs to one less in from->to */
	for(i=from;i<to;i++) {
		moveSongFromTo(i+1, i);
	}
	/* move songs to one more in to->from */
	for(i=from;i>to;i--) {
		moveSongFromTo(i-1, i);
	}
	/* put song at _to_ */
	playlist.idToPosition[tmpId] = to;
	playlist.positionToId[to] = tmpId;
	playlist.songs[to] = tmpSong;
	playlist.songMod[to] = playlist.version;
	/* now deal with order */
	if(playlist.random) {
		for(i=0;i<playlist.length;i++) {
			if(playlist.order[i]>from && playlist.order[i]<=to) {
				playlist.order[i]--;
			}
			else if(playlist.order[i]<from && 
					playlist.order[i]>=to) {
				playlist.order[i]++;
			}
			else if(from==playlist.order[i]) {
				playlist.order[i] = to;
			}
		}
	}
	else if(playlist.current==from) playlist.current = to;
	else if(playlist.current>from && playlist.current<=to) {
		playlist.current--;
	}
	else if(playlist.current>=to && playlist.current<from) {
		playlist.current++;
	}

	incrPlaylistVersion();

	return 0;
}

int moveSongInPlaylistById(FILE * fp, int id1, int to) {
	checkSongId(id1);

	return moveSongInPlaylist(fp, playlist.idToPosition[id1], to);
}

void orderPlaylist() {
	int i;

	if(playlist.current >= 0 && playlist.current < playlist.length) {
		playlist.current = playlist.order[playlist.current];
	}

	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=0) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	for(i=0;i<playlist.length;i++) {
		playlist.order[i] = i;
	}

}

void swapOrder(int a, int b) {
	int bak = playlist.order[a];
	playlist.order[a] = playlist.order[b];
	playlist.order[b] = bak;
}

void randomizeOrder(int start,int end) {
	int i;
	int ri;

	DEBUG("playlist: randomize from %i to %i\n",start,end);

	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=start && playlist.queued<=end) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	for(i=start;i<=end;i++) {
		ri = random()%(end-start+1)+start;
		if(ri==playlist.current) playlist.current = i;
		else if(i==playlist.current) playlist.current = ri;
		swapOrder(i,ri);
	}

}

int setPlaylistRandomStatus(FILE * fp, int status) {
	int statusWas = playlist.random;

	if(status!=0 && status!=1) {
		commandError(fp, ACK_ERROR_ARG, "\"%i\" is not 0 or 1", status);
		return -1;
	}

	playlist.random = status;

	if(status!=statusWas) {
		if(playlist.random) {
			/*if(playlist_state==PLAYLIST_STATE_PLAY) {
				randomizeOrder(playlist.current+1,
						playlist.length-1);
			}
			else */randomizeOrder(0,playlist.length-1);
			if(playlist.current >= 0 && 
					playlist.current < playlist.length)
			{
				swapOrder(playlist.current, 0);
				playlist.current = 0;
			}
		}
		else orderPlaylist();
	}

	return 0;
}

int previousSongInPlaylist(FILE * fp) {
	static time_t lastTime = 0;
	time_t diff = time(NULL) - lastTime;

	lastTime += diff;

	if(playlist_state!=PLAYLIST_STATE_PLAY) return 0;

	syncPlaylistWithQueue(0);

   	if (diff && getPlayerElapsedTime() > PLAYLIST_PREV_UNLESS_ELAPSED) {
		return playPlaylistOrderNumber(fp,playlist.current);
   	}
   	else {
      		if(playlist.current>0) {
			return playPlaylistOrderNumber(fp,playlist.current-1);
      		}
		else if(playlist.repeat) {
			return playPlaylistOrderNumber(fp,playlist.length-1);
		}
      		else {
         		return playPlaylistOrderNumber(fp,playlist.current);
      		}
	}

	return 0;
}

int shufflePlaylist(FILE * fp) {
	int i;
	int ri;

	if(playlist.length>1) {
		if(playlist_state==PLAYLIST_STATE_PLAY) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
			/* put current playing song first */
			swapSongs(0,playlist.order[playlist.current]);
			if(playlist.random) {
				int j;
				for(j=0;0!=playlist.order[j];j++);
				playlist.current = j;
			}
			else playlist.current = 0;
			i = 1;
		}
		else {
                        i = 0;
                        playlist.current = -1;
                }
		/* shuffle the rest of the list */
		for(;i<playlist.length;i++) {
			ri = random()%(playlist.length-1)+1;
			swapSongs(i,ri);
		}

		incrPlaylistVersion();
	}

	return 0;
}

int deletePlaylist(FILE * fp, char * utf8file) {
	char * file = utf8ToFsCharset(utf8file);
	char * rfile = malloc(strlen(file)+strlen(".")+
			strlen(PLAYLIST_FILE_SUFFIX)+1);
	char * actualFile;

	strcpy(rfile,file);
	strcat(rfile,".");
	strcat(rfile,PLAYLIST_FILE_SUFFIX);

	if((actualFile = rpp2app(rfile)) && isPlaylist(actualFile)) free(rfile);
	else {
		free(rfile);
		commandError(fp, ACK_ERROR_NO_EXIST, 
                                "playlist \"%s\" not found", utf8file);
		return -1;
	}

	if(unlink(actualFile)<0) {
		commandError(fp, ACK_ERROR_SYSTEM,
                                "problems deleting file", NULL);
		return -1;
	}

	return 0;
}

int savePlaylist(FILE * fp, char * utf8file) {
	FILE * fileP;
	int i;
	struct stat st;
	char * file;
	char * rfile;
	char * actualFile;

	if(strstr(utf8file,"/")) {
		commandError(fp, ACK_ERROR_ARG,
                                "cannot save \"%s\", saving playlists to "
				"subdirectories is not supported", utf8file);
		return -1;
	}

	file = strdup(utf8ToFsCharset(utf8file));

	rfile = malloc(strlen(file)+strlen(".")+
			strlen(PLAYLIST_FILE_SUFFIX)+1);

	strcpy(rfile,file);
	strcat(rfile,".");
	strcat(rfile,PLAYLIST_FILE_SUFFIX);

	free(file);

	actualFile = rpp2app(rfile);

	free(rfile);

	if(0==stat(actualFile,&st)) {
		commandError(fp, ACK_ERROR_EXIST, "a file or directory already " 
                                "exists with the name \"%s\"", utf8file);
		return -1;
	}

	while(!(fileP = fopen(actualFile,"w")) && errno==EINTR);
	if(fileP==NULL) {
		commandError(fp, ACK_ERROR_SYSTEM, "problems opening file", 
				NULL);
		return -1;
	}

	for(i=0;i<playlist.length;i++) {
		if(playlist_saveAbsolutePaths && 
				playlist.songs[i]->type==SONG_TYPE_FILE) 
		{
			myfprintf(fileP,"%s\n",rmp2amp(utf8ToFsCharset((
				        getSongUrl(playlist.songs[i])))));
		}
		else myfprintf(fileP,"%s\n",
				utf8ToFsCharset(getSongUrl(playlist.songs[i])));
	}

	while(fclose(fileP) && errno==EINTR);

	return 0;
}

int getPlaylistCurrentSong() {
	if(playlist.current >= 0 && playlist.current < playlist.length) {
                return playlist.order[playlist.current];
        }
        
        return -1;
}

unsigned long getPlaylistVersion() {
	return playlist.version;
}

int getPlaylistLength() {
	return playlist.length;
}

int seekSongInPlaylist(FILE * fp, int song, float time) {
	int i = song;

	if(song<0 || song>=playlist.length) {
		commandError(fp, ACK_ERROR_NO_EXIST,
                                "song doesn't exist: \"%i\"", song);
		return -1;
	}

	if(playlist.random) for(i=0;song!=playlist.order[i];i++);

	clearPlayerError();
	playlist_stopOnError = 1;
	playlist_errorCount = 0;

	if(playlist_state == PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=0) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}
	else if(playPlaylistOrderNumber(fp,i)<0) return -1;

	if(playlist.current!=i) {
		if(playPlaylistOrderNumber(fp,i)<0) return -1;
	}

	return playerSeek(fp, playlist.songs[playlist.order[i]], time);
}

int seekSongInPlaylistById(FILE * fp, int id, float time) {
	checkSongId(id);

	return seekSongInPlaylist(fp, playlist.idToPosition[id], time);
}

int getPlaylistSongId(int song) {
	return playlist.positionToId[song];
}

static int PlaylistIterFunc(FILE * fp, char * utf8file, void (*IterFunc)(FILE *fp, char *utf8_file, char **errored_File)) {
	FILE * fileP;
	char s[MAXPATHLEN+1];
	int slength = 0;
	char * temp = strdup(utf8ToFsCharset(utf8file));
	char * rfile = malloc(strlen(temp)+strlen(".")+
			strlen(PLAYLIST_FILE_SUFFIX)+1);
	char * actualFile;
	char * parent = parentPath(temp);
	int parentlen = strlen(parent);
	char * erroredFile = NULL;
	int tempInt;
	int commentCharFound = 0;

	strcpy(rfile,temp);
	strcat(rfile,".");
	strcat(rfile,PLAYLIST_FILE_SUFFIX);

	free(temp);

	if((actualFile = rpp2app(rfile)) && isPlaylist(actualFile)) free(rfile);
	else {
		free(rfile);
		commandError(fp, ACK_ERROR_NO_EXIST,
				"playlist \"%s\" not found", utf8file);
		return -1;
	}

	while(!(fileP = fopen(actualFile,"r")) && errno==EINTR);
	if(fileP==NULL) {
		commandError(fp, ACK_ERROR_SYSTEM,
				"problems opening file \"%s\"", utf8file);
		return -1;
	}

	while((tempInt = fgetc(fileP))!=EOF) {
		s[slength] = tempInt;
		if(s[slength]=='\n' || s[slength]=='\0') {
			commentCharFound = 0;
			s[slength] = '\0';
			if(s[0]==PLAYLIST_COMMENT) {
				commentCharFound = 1;
			}
			if(strncmp(s,musicDir,strlen(musicDir))==0) {
				strcpy(s,&(s[strlen(musicDir)]));
			}
			else if(parentlen) {
				temp = strdup(s);
				memset(s,0,MAXPATHLEN+1);
				strcpy(s,parent);
				strncat(s,"/",MAXPATHLEN-parentlen);
				strncat(s,temp,MAXPATHLEN-parentlen-1);
				if(strlen(s)>=MAXPATHLEN) {
					commandError(fp, 
							ACK_ERROR_PLAYLIST_LOAD,
							"\"%s\" too long",
							temp);
					free(temp);
					while(fclose(fileP) && errno==EINTR);
					if(erroredFile) free(erroredFile);
					return -1;
				}
				free(temp);
			}
			slength = 0;
			temp = fsCharsetToUtf8(s);
			if(!temp) continue;
			if(!commentCharFound)
			{
				/* using temp directly should be safe,
				 * for our current IterFunction set
				 * but just in case, we copy to s */
				strcpy(s, temp);
				IterFunc(fp, s, &erroredFile);
			}
		}
		else if(slength==MAXPATHLEN) {
			s[slength] = '\0';
			commandError(fp, ACK_ERROR_PLAYLIST_LOAD,
					"line in \"%s\" is too long", utf8file);
			ERROR("line \"%s\" in playlist \"%s\" is too long\n",
					s, utf8file);
			while(fclose(fileP) && errno==EINTR);
			if(erroredFile) free(erroredFile);
			return -1;
		}
		else if(s[slength]!='\r') slength++;
	}

	while(fclose(fileP) && errno==EINTR);

	if(erroredFile) {
		commandError(fp, ACK_ERROR_PLAYLIST_LOAD,
				"can't add file \"%s\"", erroredFile);
		free(erroredFile);
		return -1;
	}

	return 0;
}


static void PlaylistInfoPrintInfo(FILE *fp, char *utf8file, char **erroredfile) {
	Song * song = getSongFromDB(utf8file);
	if(song) {
		printSongInfo(fp, song);       	
	}
	else {
		myfprintf(fp,"file: %s\n",utf8file);
	}                                  	
}
static void PlaylistInfoPrint(FILE *fp, char *utf8file, char **erroredfile) {
	myfprintf(fp,"file: %s\n",utf8file);
}

static void PlaylistLoadIterFunc(FILE *fp, char *temp, char **erroredFile) {
	if(!getSongFromDB(temp)	&& !isRemoteUrl(temp)) 
	{
		
	}
	else if((addToPlaylist(stderr, temp, 0))<0) {
		/* for windows compatibilit, convert slashes */
		char * temp2 = strdup(temp);
		char * p = temp2;
		while(*p) {
			if(*p=='\\') *p = '/';
			p++;
		}
		if((addToPlaylist(stderr, temp2, 0))<0) {
			if(!*erroredFile) {
				*erroredFile = strdup(temp);
			}
		}
		free(temp2);
	}
}

int PlaylistInfo(FILE * fp, char * utf8file, int detail) {
	if(detail) {
		return PlaylistIterFunc(fp, utf8file, PlaylistInfoPrintInfo);
	}
	return PlaylistIterFunc(fp, utf8file, PlaylistInfoPrint) ;
}

int loadPlaylist(FILE * fp, char * utf8file) {
	return PlaylistIterFunc(fp, utf8file, PlaylistLoadIterFunc);
}
