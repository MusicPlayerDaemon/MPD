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

#define BITS_FOR_VERSION	31

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

typedef struct _Playlist {
	Song ** songs;
	int * order;
	int length;
	int current;
	int queued;
	int repeat;
	int random;
	unsigned long version;
} Playlist;

Playlist playlist;
int playlist_state = PLAYLIST_STATE_STOP;
int playlist_max_length;
int playlist_stopOnError;
int playlist_errorCount = 0;
int playlist_queueError;
int playlist_noGoToNext = 0;

int playlist_saveAbsolutePaths;

char * playlist_stateFile = NULL;

void swapOrder(int a, int b);
int playPlaylistOrderNumber(FILE * fp, int orderNum);
void randomizeOrder(int start, int end);

void incrPlaylistVersion() {
	static unsigned long max = ((unsigned long)1<<BITS_FOR_VERSION)-1;
	playlist.version++;
	if(playlist.version>=max) playlist.version = 0;
}

void initPlaylist() {
	char * test;

	playlist.length = 0;
	playlist.repeat = 0;
	playlist.version = 0;
	playlist.random = 0;
	playlist.queued = -1;

	blockTermSignal();

	playlist_max_length = strtol((getConf())[CONF_MAX_PLAYLIST_LENGTH],&test,10);
	if(*test!='\0') {
		ERROR("max playlist length \"%s\" is not an integer\n",
				(getConf())[CONF_MAX_PLAYLIST_LENGTH]);
		exit(EXIT_FAILURE);
	}

	if(strcmp("yes",(getConf())[CONF_SAVE_ABSOLUTE_PATHS_IN_PLAYLISTS])
			==0) {
		playlist_saveAbsolutePaths = 1;
	}
	else if(strcmp("no",(getConf())[CONF_SAVE_ABSOLUTE_PATHS_IN_PLAYLISTS])
			==0) {
		playlist_saveAbsolutePaths = 0;
	}
	else {
		ERROR("save_absolute_paths_in_playlist \"%s\" is not yes or "
			"no\n",
			(getConf())[CONF_SAVE_ABSOLUTE_PATHS_IN_PLAYLISTS]);
		exit(EXIT_FAILURE);
	}

	playlist.songs = malloc(sizeof(Song *)*playlist_max_length);
	playlist.order = malloc(sizeof(Song *)*playlist_max_length);

	memset(playlist.songs,0,sizeof(char *)*playlist_max_length);

	srand(time(NULL));

	if(getConf()[CONF_STATE_FILE]) {
		playlist_stateFile = getConf()[CONF_STATE_FILE];
	}

	unblockTermSignal();
}

void finishPlaylist() {
	clearPlaylist(stderr);
	free(playlist.songs);
	playlist.songs = NULL;
	free(playlist.order);
	playlist.order = NULL;
}

int clearPlaylist(FILE * fp) {
	int i;

	if(stopPlaylist(fp)<0) return -1;

	blockTermSignal();
	for(i=0;i<playlist.length;i++) playlist.songs[i] = NULL;
	playlist.length = 0;
	unblockTermSignal();

	incrPlaylistVersion();

	return 0;
}

int showPlaylist(FILE * fp) {
	int i;

	for(i=0;i<playlist.length;i++) {
		myfprintf(fp,"%i:%s\n",i,(playlist.songs[i])->utf8file);
	}

	return 0;
}

void savePlaylistState() {
	if(playlist_stateFile) {
		FILE * fp;

		blockTermSignal();
		while(!(fp = fopen(playlist_stateFile,"w")) && errno==EINTR);
		if(!fp) {
			ERROR("problems opening state file \"%s\" for "
				"writing\n",playlist_stateFile);
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
		unblockTermSignal();
	}
}

void loadPlaylistFromStateFile(FILE * fp, char * buffer, int state, int current,
		int time) 
{
	char * temp;
	int song;

	if(!myFgets(buffer,PLAYLIST_BUFFER_SIZE,fp)) {
		ERROR("error parsing state file \"%s\"\n",playlist_stateFile);
		exit(EXIT_FAILURE);
	}
	while(strcmp(buffer,PLAYLIST_STATE_FILE_PLAYLIST_END)) {
		song = atoi(strtok(buffer,":"));
		if(!(temp = strtok(NULL,""))) {
			ERROR("error parsing state file \"%s\"\n",
					playlist_stateFile);
			exit(EXIT_FAILURE);
		}
		if(addToPlaylist(stderr,temp)==0 && current==song) {
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
			ERROR("error parsing state file \"%s\"\n",
					playlist_stateFile);
			exit(EXIT_FAILURE);
		}
	}
}

void readPlaylistState() {
	if(playlist_stateFile) {
		FILE * fp;
		struct stat st;
		int current = -1;
		int time = 0;
		int state = PLAYER_STATE_STOP;
		char buffer[PLAYLIST_BUFFER_SIZE];

		if(stat(playlist_stateFile,&st)<0) return;
		if(!S_ISREG(st.st_mode)) {
			ERROR("state file \"%s\" is not a regular "
				"file\n",playlist_stateFile);
			exit(EXIT_FAILURE);
		}

		fp = fopen(playlist_stateFile,"r");
		if(!fp) {
			ERROR("problems opening state file \"%s\" for "
				"reading\n",playlist_stateFile);
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
						playlist_stateFile);
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

int playlistInfo(FILE * fp,int song) {
	MpdTag * tag;
	int i;
	int begin = 0;
	int end = playlist.length;

	if(song>=0) {
		begin = song;
		end = song+1;
	}
	if(song>=playlist.length) {
		myfprintf(fp,"%s song doesn't exist\n",COMMAND_RESPOND_ERROR);
		return -1;
	}

	for(i=begin;i<end;i++) {
		myfprintf(fp,"file: %s\n",(playlist.songs[i])->utf8file);
		if((tag = (playlist.songs[i])->tag)) {
			printMpdTag(fp,tag);
		}
	} 

	return 0;
}

void swapSongs(int song1, int song2) {
	Song * temp;
	
	temp = playlist.songs[song1];
	playlist.songs[song1] = playlist.songs[song2];
	playlist.songs[song2] = temp;
}

void queueNextSongInPlaylist() {
	if(playlist.current<playlist.length-1) {
		playlist.queued = playlist.current+1;
		DEBUG("playlist: queue song %i:\"%s\"\n",
				playlist.queued,
				playlist.songs[playlist.order[
				playlist.queued]]->utf8file);
		if(queueSong(playlist.songs[playlist.order[
				playlist.queued]]->utf8file)<0) {
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
				playlist.songs[playlist.order[
				playlist.queued]]->utf8file);
		if(queueSong(playlist.songs[playlist.order[
				playlist.queued]]->utf8file)<0) {
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

int addToPlaylist(FILE * fp, char * file) {
	Song * song;

	DEBUG("add to playlist: %s\n",file);
	
	if(!(song = getSong(file))) {
		myfprintf(fp,"%s \"%s\" is not in the music db\n",COMMAND_RESPOND_ERROR,file);
		return -1;
	}

	return addSongToPlaylist(fp,song);
}

int addSongToPlaylist(FILE * fp, Song * song) {
	if(playlist.length==playlist_max_length) {
		myfprintf(fp,"%s playlist is at the max size\n",COMMAND_RESPOND_ERROR);
		return -1;
	}

	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=0 && playlist.current==playlist.length-1) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	playlist.songs[playlist.length] = song;
	playlist.order[playlist.length] = playlist.length;
	playlist.length++;

	if(playlist.random) {
		int swap;
		int start;
		if(playlist_state==PLAYLIST_STATE_STOP) start = 0;
		else if(playlist.queued>=0) start = playlist.queued+1;
		else start = playlist.current+1;
		swap = rand()%(playlist.length-start);
		swap+=start;
		swapOrder(playlist.length-1,swap);
	}
	
	incrPlaylistVersion();

	return 0;
}

int swapSongsInPlaylist(FILE * fp, int song1, int song2) {
	int queuedSong = -1;
	int currentSong = -1;

	if(song1<0 || song1>=playlist.length) {
		fprintf(fp,"%s \"%i\" is not in the playlist\n",
				COMMAND_RESPOND_ERROR,song1);
		return -1;
	}
	if(song2<0 || song2>=playlist.length) {
		fprintf(fp,"%s \"%i\" is not in the playlist\n",
				COMMAND_RESPOND_ERROR,song2);
		return -1;
	}

	blockTermSignal();
	
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

	unblockTermSignal();

	incrPlaylistVersion();

	return 0;
}

int deleteFromPlaylist(FILE * fp, int song) {
	int i;
	int songOrder;

	if(song<0) {
		myfprintf(fp,"%s need a positive integer\n",COMMAND_RESPOND_ERROR);
		return -1;
	}
	if(song>=playlist.length) {
		myfprintf(fp,"%s song doesn't exist\n",COMMAND_RESPOND_ERROR);
		return -1;
	}

	blockTermSignal();
	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=0 && (playlist.order[playlist.queued]==song
			|| playlist.order[playlist.current]==song)) 
		{
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();	
		}
	}

	/* delete song from songs array */
	for(i=song;i<playlist.length-1;i++) {
		playlist.songs[i] = playlist.songs[i+1];
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

	unblockTermSignal();

	incrPlaylistVersion();

	if(playlist_state!=PLAYLIST_STATE_STOP && playlist.current==songOrder) {
		/*if(playlist.current>=playlist.length) return playerStop(fp);
		else return playPlaylistOrderNumber(fp,playlist.current);*/
		playerStop(stderr);
		playlist_noGoToNext = 1;
	}
	else if(playlist_state!=PLAYLIST_STATE_STOP && 
			playlist.current>songOrder) {
		playlist.current--;
	}

	if(playlist_state!=PLAYLIST_STATE_STOP && playlist.queued>songOrder) {
		playlist.queued--;
	}

	return 0;
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
	playlist.current = orderNum;

	DEBUG("playlist: play %i:\"%s\"\n",orderNum,
			(playlist.songs[playlist.order[orderNum]])->utf8file);

	if(playerPlay(fp,(playlist.songs[playlist.order[orderNum]])->
			utf8file)<0) 
	{
		stopPlaylist(fp);
		return -1;
	}

	return 0;
}

int playPlaylist(FILE * fp, int song, int stopOnError) {
	int i = song;

	clearPlayerError();

	if(song==-1) i = 0;
	else if(song<0) {
		myfprintf(fp,"%s need integer >= -1\n",COMMAND_RESPOND_ERROR);
		playlist_state = PLAYLIST_STATE_STOP;
		return -1;
	}
	if(!playlist.length) {
		myfprintf(fp,"%s playlist is empty\n",COMMAND_RESPOND_ERROR);
		playlist_state = PLAYLIST_STATE_STOP;
		return -1;
	}
	else if(song>=playlist.length) {
		myfprintf(fp,"%s song doesn't exist\n",COMMAND_RESPOND_ERROR);
		playlist_state = PLAYLIST_STATE_STOP;
		return -1;
	}

	if(playlist.random) {
		/*if(song == -1 && playlist_state==PLAYLIST_STATE_PLAY) {
			randomizeOrder(0,playlist.length-1);
		}
		else {*/
			if(song>=0) for(i=0;song!=playlist.order[i];i++);
			if(playlist_state==PLAYLIST_STATE_STOP) {
				playlist.current = 0;
			}
			swapOrder(i,playlist.current);
			i = playlist.current;
		/*}*/
	}

	playlist_stopOnError = stopOnError;
	playlist_errorCount = 0;

	return playPlaylistOrderNumber(fp,i);
}

void syncPlayerAndPlaylist() {
	if(playlist_state!=PLAYLIST_STATE_PLAY) return;

	if(getPlayerState()==PLAYER_STATE_STOP) playPlaylistIfPlayerStopped();
	else syncPlaylistWithQueue(!playlist_queueError);
}

int currentSongInPlaylist(FILE * fp) {
	if(playlist_state!=PLAYLIST_STATE_PLAY) return 0;

	playlist_stopOnError = 0;

	syncPlaylistWithQueue(0);

	if(playlist.current<playlist.length) {
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
		myfprintf(fp,"%s \"%i\" is not 0 or 1\n",COMMAND_RESPOND_ERROR,status);
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
	int queuedSong = -1;
	int currentSong = -1;

	if(from<0 || from>=playlist.length) {
		fprintf(fp,"%s \"%i\" is not a song in the playlist\n",
				COMMAND_RESPOND_ERROR,from);
		return -1;
	}

	if(to<0 || to>=playlist.length) {
		fprintf(fp,"%s \"%i\" is not a song in the playlist\n",
				COMMAND_RESPOND_ERROR,to);
		return -1;
	}

	blockTermSignal();
	
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
	/* move songs to one less in from->to */
	for(i=from;i<to;i++) playlist.songs[i] = playlist.songs[i+1];
	/* move songs to one more in to->from */
	for(i=from;i>to;i--) playlist.songs[i] = playlist.songs[i-1];
	/* put song at _to_ */
	playlist.songs[to] = tmpSong;
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

	unblockTermSignal();

	incrPlaylistVersion();

	return 0;
}

void orderPlaylist() {
	int i;

	playlist.current = playlist.order[playlist.current];

	blockTermSignal();
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

	unblockTermSignal();
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

	blockTermSignal();

	if(playlist_state==PLAYLIST_STATE_PLAY) {
		if(playlist.queued>=start && playlist.queued<=end) {
			lockPlaylistInteraction();
			clearPlayerQueue();
			unlockPlaylistInteraction();
		}
	}

	for(i=start;i<=end;i++) {
		ri = rand()%(end-start+1)+start;
		if(ri==playlist.current) playlist.current = i;
		else if(i==playlist.current) playlist.current = ri;
		swapOrder(i,ri);
	}

	unblockTermSignal();
}

int setPlaylistRandomStatus(FILE * fp, int status) {
	int statusWas = playlist.random;

	if(status!=0 && status!=1) {
		myfprintf(fp,"%s \"%i\" is not 0 or 1\n",COMMAND_RESPOND_ERROR,status);
		return -1;
	}

	playlist.random = status;

	if(status!=statusWas) {
		if(playlist.random) {
			if(playlist_state==PLAYLIST_STATE_PLAY) {
				randomizeOrder(playlist.current+1,
						playlist.length-1);
			}
			else randomizeOrder(0,playlist.length-1);
		}
		else orderPlaylist();
	}

	return 0;
}

int previousSongInPlaylist(FILE * fp) {
	if(playlist_state!=PLAYLIST_STATE_PLAY) return 0;

	syncPlaylistWithQueue(0);

   	if (getPlayerElapsedTime()>PLAYLIST_PREV_UNLESS_ELAPSED) {
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
		blockTermSignal();
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
		else i = 0;
		/* shuffle the rest of the list */
		for(;i<playlist.length;i++) {
			ri = rand()%(playlist.length-1)+1;
			swapSongs(i,ri);
		}
		unblockTermSignal();

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
		myfprintf(fp,"%s playlist \"%s\" not found\n",
				COMMAND_RESPOND_ERROR,utf8file);
		return -1;
	}

	if(unlink(actualFile)<0) {
		myfprintf(fp,"%s problems deleting file\n",COMMAND_RESPOND_ERROR);
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
		myfprintf(fp,"%s cannot save \"%s\", saving playlists to "
				"subdirectories is not supported\n",
				COMMAND_RESPOND_ERROR,utf8file);
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
		myfprintf(fp,"%s A file or directory already exists with the name \"%s\"\n",COMMAND_RESPOND_ERROR,utf8file);
		return -1;
	}


	while(!(fileP = fopen(actualFile,"w")) && errno==EINTR);
	if(fileP==NULL) {
		myfprintf(fp,"%s Problems opening file\n",COMMAND_RESPOND_ERROR);
		return -1;
	}

	for(i=0;i<playlist.length;i++) {
		if(playlist_saveAbsolutePaths) {
			myfprintf(fileP,"%s%s\n",musicDir,
				utf8ToFsCharset((playlist.songs[i])->utf8file));
		}
		else myfprintf(fileP,"%s\n",
				utf8ToFsCharset((playlist.songs[i])->utf8file));
	}

	while(fclose(fileP) && errno==EINTR);

	return 0;
}

int loadPlaylist(FILE * fp, char * utf8file) {
	FILE * fileP;
	char s[MAXPATHLEN*1];
	int slength = 0;
	char * temp = strdup(utf8ToFsCharset(utf8file));
	char * rfile = malloc(strlen(temp)+strlen(".")+
			strlen(PLAYLIST_FILE_SUFFIX)+1);
	char * actualFile;
	char * parent = parentPath(temp);
	int parentlen = strlen(parent);
	char * erroredFile = NULL;
	int tempInt;

	strcpy(rfile,temp);
	strcat(rfile,".");
	strcat(rfile,PLAYLIST_FILE_SUFFIX);

	free(temp);

	if((actualFile = rpp2app(rfile)) && isPlaylist(actualFile)) free(rfile);
	else {
		free(rfile);
		myfprintf(fp,"%s playlist \"%s\" not found\n",
				COMMAND_RESPOND_ERROR,utf8file);
		return -1;
	}

	while(!(fileP = fopen(actualFile,"r")) && errno==EINTR);
	if(fileP==NULL) {
		myfprintf(fp,"%s Problems opening file \"%s\"\n",
				COMMAND_RESPOND_ERROR,utf8file);
		return -1;
	}

	while((tempInt = fgetc(fileP))!=EOF) {
		s[slength] = tempInt;
		if(s[slength]=='\n' || s[slength]=='\0') {
			s[slength] = '\0';
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
					myfprintf(fp,"%s \"%s\" too long\n",COMMAND_RESPOND_ERROR,temp);
					free(temp);
					while(fclose(fileP) && errno==EINTR);
					if(erroredFile) free(erroredFile);
					return -1;
				}
				free(temp);
			}
			slength = 0;
			temp = strdup(fsCharsetToUtf8(s));
			if(s[0]==PLAYLIST_COMMENT && !getSong(temp)) {
				free(temp);
				continue;
			}
			if((addToPlaylist(stderr,temp))<0) {
				if(!erroredFile) erroredFile = strdup(temp);
			}
			free(temp);
		}
		else if(slength==MAXPATHLEN) {
			s[slength] = '\0';
			myfprintf(fp,"%s \"%s\" too long\n",COMMAND_RESPOND_ERROR,s);
			while(fclose(fileP) && errno==EINTR);
			if(erroredFile) free(erroredFile);
			return -1;
		}
		else if(s[slength]!='\r') slength++;
	}

	while(fclose(fileP) && errno==EINTR);

	if(erroredFile) {
		myfprintf(fp,"%s can't add file \"%s\"\n",COMMAND_RESPOND_ERROR,
				erroredFile);
		free(erroredFile);
		return -1;
	}

	return 0;
}

int getPlaylistCurrentSong() {
	return playlist.order[playlist.current];
}

unsigned long getPlaylistVersion() {
	return playlist.version;
}

int getPlaylistLength() {
	return playlist.length;
}

int seekSongInPlaylist(FILE * fp, int song, float time) {
	int i = song;

	if(song<0) {
		myfprintf(fp,"%s need integer >= -1\n",COMMAND_RESPOND_ERROR);
		return -1;
	}
	if(!playlist.length) {
		myfprintf(fp,"%s playlist is empty\n",COMMAND_RESPOND_ERROR);
		return -1;
	}
	else if(song>=playlist.length) {
		myfprintf(fp,"%s song doesn't exist\n",COMMAND_RESPOND_ERROR);
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

	return playerSeek(fp,playlist.songs[playlist.order[i]]->utf8file,time);
}
