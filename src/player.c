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

#include "player.h"
#include "decode.h"
#include "command.h"
#include "interface.h"
#include "playlist.h"
#include "ls.h"
#include "listen.h"
#include "log.h"
#include "utils.h"
#include "tables.h"
#include "directory.h"
#include "volume.h"
#include "playerData.h"
#include "permission.h"
#include "sig_handlers.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

volatile int player_pid = 0;

void clearPlayerPid() {
	player_pid = 0;
}

void resetPlayer() {
	int pid;

	clearPlayerPid();
	getPlayerData()->playerControl.stop = 0;
	getPlayerData()->playerControl.play = 0;
	getPlayerData()->playerControl.pause = 0;
	getPlayerData()->playerControl.lockQueue = 0;
	getPlayerData()->playerControl.unlockQueue = 0;
	getPlayerData()->playerControl.state = PLAYER_STATE_STOP;
	getPlayerData()->playerControl.queueState = PLAYER_QUEUE_UNLOCKED;
	getPlayerData()->playerControl.seek = 0;
	/* kill decode process if it got left running */
	pid = getPlayerData()->playerControl.decode_pid;
	if(pid>0) kill(pid,SIGTERM);
	getPlayerData()->playerControl.decode_pid = 0;
}

void player_sigChldHandler(int pid, int status) {
	if(player_pid==pid) {
		DEBUG("SIGCHLD caused by player process\n");
		if(WIFSIGNALED(status) && WTERMSIG(status)!=SIGTERM &&
				WTERMSIG(status)!=SIGINT) 
		{
			ERROR("player process died from signal: %i\n",
					WTERMSIG(status));
		}
		resetPlayer();
	}
	else if(pid==getPlayerData()->playerControl.decode_pid && player_pid<=0)
	{
		if(WIFSIGNALED(status) && WTERMSIG(status)!=SIGTERM) {
			ERROR("(caught by master parent) "
					"decode process died from a "
					"non-TERM signal: %i\n",
					WTERMSIG(status));
		}
		getPlayerData()->playerControl.decode_pid = 0;
	}
}

int playerInit() {
	blockSignals();
	player_pid = fork();
	if(player_pid==0) {
		PlayerControl * pc = &(getPlayerData()->playerControl);
		struct sigaction sa;

		clearUpdatePid();

		unblockSignals();

		sa.sa_flags = 0;
		sigemptyset(&sa.sa_mask);

		finishSigHandlers();
		sa.sa_handler = decodeSigHandler;
		while(sigaction(SIGCHLD,&sa,NULL)<0 && errno==EINTR);
		while(sigaction(SIGTERM,&sa,NULL)<0 && errno==EINTR);
		while(sigaction(SIGINT,&sa,NULL)<0 && errno==EINTR);

		while(close(listenSocket)<0 && errno==EINTR);
		freeAllInterfaces();
		closeMp3Directory();
		finishPlaylist();
		closeTables();
		finishPermissions();
		finishCommands();
		finishVolume();

		while(1) {
			if(pc->play) decode();
			else if(pc->stop) pc->stop = 0;
			else if(pc->pause) pc->pause = 0;
			else if(pc->closeAudio) {
				closeAudioDevice();
				pc->closeAudio = 0;
				kill(getppid(),SIGUSR1);
			}
			else if(pc->lockQueue) {
				pc->queueLockState = PLAYER_QUEUE_LOCKED;
				pc->lockQueue = 0;
			}
			else if(pc->unlockQueue) {
				pc->queueLockState = PLAYER_QUEUE_UNLOCKED;
				pc->unlockQueue = 0;
			}
                        else if(pc->cycleLogFiles) {
                                myfprintfCloseAndOpenLogFile();
                                pc->cycleLogFiles = 0;
                        }
			else my_usleep(10000);
		}

		exit(EXIT_SUCCESS);
	}
	else if(player_pid<0) {
		unblockSignals();
		ERROR("player Problems fork()'ing\n");
		player_pid = 0;

		return -1;
	}
	unblockSignals();

	return 0;
}

int playerPlay(FILE * fp, Song * song) {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(fp==NULL) fp = stderr;

	if(playerStop(fp)<0) return -1;

	/*{
		struct stat st;
		if(stat(rmp2amp(utf8ToFsCharset(utf8file)),&st)<0) {
			strncpy(pc->erroredFile,pc->file,MAXPATHLEN);
			pc->erroredFile[MAXPATHLEN] = '\0';
			pc->error = PLAYER_ERROR_FILENOTFOUND;
			return 0;
		}
	}*/
	
        if(song->tag) pc->fileTime = song->tag->time;
        else pc->fileTime = 0;

        strncpy(pc->utf8url, song->utf8url, MAXPATHLEN);
	pc->utf8url[MAXPATHLEN] = '\0';

	pc->play = 1;
	if(player_pid==0 && playerInit()<0) {
		pc->play = 0;
		return -1;
	}
	
	while(player_pid>0 && pc->play) my_usleep(1000);
	
	return 0;
}

int playerStop(FILE * fp) {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(player_pid>0 && pc->state!=PLAYER_STATE_STOP) {
		pc->stop = 1;
		while(player_pid>0 && pc->stop) my_usleep(1000);
	}

	pc->queueState = PLAYER_QUEUE_BLANK;
	playerQueueUnlock();

	return 0;
}

void playerKill() {
	int pid;
	/*PlayerControl * pc = &(getPlayerData()->playerControl);

	playerStop(stderr);
	playerCloseAudio(stderr);
	if(player_pid>0 && pc->closeAudio) sleep(1);*/

	pid = player_pid;
	if(pid>0) kill(pid,SIGTERM);
}

int playerPause(FILE * fp) {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(player_pid>0 && pc->state!=PLAYER_STATE_STOP) {
		pc->pause = 1;
		while(player_pid>0 && pc->pause) my_usleep(1000);
	}

	return 0;
}

int playerSetPause(FILE * fp, int pause) {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(player_pid<=0) return 0;

	switch(pc->state) {
	case PLAYER_STATE_PLAY:
		if(pause) playerPause(fp);
		break;
	case PLAYER_STATE_PAUSE:
		if(!pause) playerPause(fp);
		break;
	}

	return 0;
}

int getPlayerElapsedTime() {
	return (int)(getPlayerData()->playerControl.elapsedTime+0.5);
}

unsigned long getPlayerBitRate() {
	return getPlayerData()->playerControl.bitRate;
}

int getPlayerTotalTime() {
	return (int)(getPlayerData()->playerControl.totalTime+0.5);
}

int getPlayerState() {
	return getPlayerData()->playerControl.state;
}

void clearPlayerError() {
	getPlayerData()->playerControl.error = 0;
}

int getPlayerError() {
	return getPlayerData()->playerControl.error;
}

char * getPlayerErrorStr() {
	static char * error = NULL;
	int errorlen = MAXPATHLEN+1024;
	PlayerControl * pc = &(getPlayerData()->playerControl);

	error = realloc(error,errorlen+1);
	memset(error,0,errorlen+1);

	switch(pc->error) {
	case PLAYER_ERROR_FILENOTFOUND:
		snprintf(error,errorlen,
				"file \"%s\" does not exist or is inaccesible",
				pc->erroredUrl);
		break;
	case PLAYER_ERROR_FILE:
		snprintf(error,errorlen,"problems decoding \"%s\"",
				pc->erroredUrl);
		break;
	case PLAYER_ERROR_AUDIO:
		snprintf(error,errorlen,"problems opening audio device");
		break;
	case PLAYER_ERROR_SYSTEM:
		snprintf(error,errorlen,"system error occured");
		break;
	case PLAYER_ERROR_UNKTYPE:
		snprintf(error,errorlen,"file type  of \"%s\" is unknown",
				pc->erroredUrl);
	default:
		break;
	}

	errorlen = strlen(error);
	error = realloc(error,errorlen+1);

	if(errorlen) return error;

	return NULL;
}

void playerCloseAudio() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(player_pid>0) {
		if(playerStop(stderr)<0) return;
		pc->closeAudio = 1;
	}
}

int queueSong(Song * song) {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(pc->queueState==PLAYER_QUEUE_BLANK) {
                strncpy(pc->utf8url, song->utf8url, MAXPATHLEN);
		pc->utf8url[MAXPATHLEN] = '\0';

                if(song->tag) pc->fileTime = song->tag->time;
                else pc->fileTime = 0;

		pc->queueState = PLAYER_QUEUE_FULL;
		return 0;
	}

	return -1;
}

int getPlayerQueueState() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	return pc->queueState;
}

void setQueueState(int queueState) {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	pc->queueState = queueState;
}

void playerQueueLock() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(player_pid>0 && pc->queueLockState==PLAYER_QUEUE_UNLOCKED)
	{
		pc->lockQueue = 1;
		while(player_pid>0 && pc->lockQueue) my_usleep(1000);
	}
}

void playerQueueUnlock() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(player_pid>0 && pc->queueLockState==PLAYER_QUEUE_LOCKED)
	{
		pc->unlockQueue = 1;
		while(player_pid>0 && pc->unlockQueue) my_usleep(1000);
	}
}

int playerSeek(FILE * fp, Song * song, float time) {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(pc->state==PLAYER_STATE_STOP) {
		myfprintf(fp,"%s player not currently playing\n",
				COMMAND_RESPOND_ERROR);
		return -1;
	}

	if(strcmp(pc->utf8url, song->utf8url)!=0) {
                if(song->tag) pc->fileTime = song->tag->time;
                else pc->fileTime = 0;

		strncpy(pc->utf8url, song->utf8url, MAXPATHLEN);
		pc->utf8url[MAXPATHLEN] = '\0';
	}

	if(pc->error==PLAYER_ERROR_NOERROR) {
		pc->seekWhere = time;
		pc->seek = 1;
		while(player_pid>0 && pc->seek) my_usleep(1000);
	}

	return 0;
}

float getPlayerCrossFade() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	return pc->crossFade;
}

void setPlayerCrossFade(float crossFadeInSeconds) {
	PlayerControl * pc;
	if(crossFadeInSeconds<0) crossFadeInSeconds = 0;

	pc = &(getPlayerData()->playerControl);

	pc->crossFade = crossFadeInSeconds;
}

void setPlayerSoftwareVolume(int volume) {
	PlayerControl * pc;
	volume = (volume>1000) ? 1000 : (volume<0 ? 0 : volume);

	pc = &(getPlayerData()->playerControl);

	pc->softwareVolume = volume;
}

int getPlayerSoftwareVolume() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	return pc->softwareVolume;
}

double getPlayerTotalPlayTime() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	if(pc->state==PLAYER_STATE_STOP) {
		return pc->totalPlayTime;
	}

	return pc->totalPlayTime+pc->elapsedTime-pc->beginTime;
}

unsigned int getPlayerSampleRate() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	return pc->sampleRate;
}

int getPlayerBits() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	return pc->bits;
}

int getPlayerChannels() {
	PlayerControl * pc = &(getPlayerData()->playerControl);

	return pc->channels;
}

void playerCycleLogFiles() {
	PlayerControl * pc = &(getPlayerData()->playerControl);
	DecoderControl * dc = &(getPlayerData()->decoderControl);

	pc->cycleLogFiles = 1;
	dc->cycleLogFiles = 1;
}

/* this actually creates a dupe of the current metadata */
Song * playerCurrentDecodeSong() {
        static Song * song;
	DecoderControl * dc = &(getPlayerData()->decoderControl);

        if(dc->metadataSet && (!song || strcmp(song->utf8url, dc->utf8url))) {
                if(!song) {
                        song = newNullSong();
                        song->tag = newMpdTag();
                }
                if(song->utf8url) free(song->utf8url);
                song->utf8url = strdup(dc->utf8url);
                if(dc->title >= 0) {
                        song->tag->title = dc->title + dc->metadata;
                }
                else song->tag->title = NULL;

                return song;
        }

        return NULL;
}

/* vim:set shiftwidth=4 tabstop=8 expandtab: */
