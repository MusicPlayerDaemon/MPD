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

#include "player.h"
#include "path.h"
#include "decode.h"
#include "command.h"
#include "interface.h"
#include "playlist.h"
#include "ls.h"
#include "listen.h"
#include "log.h"
#include "utils.h"
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

void clearPlayerPid(void)
{
	player_pid = 0;
}

static void resetPlayerMetadata(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (pc->metadataState == PLAYER_METADATA_STATE_READ) {
		pc->metadataState = PLAYER_METADATA_STATE_WRITE;
	}
}

static void resetPlayer(void)
{
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
	getPlayerData()->playerControl.metadataState =
	    PLAYER_METADATA_STATE_WRITE;
	pid = getPlayerData()->playerControl.decode_pid;
	if (pid > 0)
		kill(pid, SIGTERM);
	getPlayerData()->playerControl.decode_pid = 0;
}

void player_sigChldHandler(int pid, int status)
{
	if (player_pid == pid) 
	{
		/*
		DEBUG("SIGCHLD caused by player process\n");
		if (WIFSIGNALED(status) && 
		    WTERMSIG(status) != SIGTERM &&
		    WTERMSIG(status) != SIGINT) 
		{
			ERROR("player process died from signal: %i\n",
			      WTERMSIG(status));
		}
		*/
		resetPlayer();
	} 
	else if (pid == getPlayerData()->playerControl.decode_pid &&
		 player_pid <= 0) 
	{
		/*
		if (WIFSIGNALED(status) && WTERMSIG(status) != SIGTERM) 
		{
			ERROR("(caught by master parent) "
			      "decode process died from a "
			      "non-TERM signal: %i\n", WTERMSIG(status));
		}
		*/
		getPlayerData()->playerControl.decode_pid = 0;
	}
}

int playerInit(void)
{
	blockSignals();
	player_pid = fork();
	if (player_pid==0)
	{
		clock_t start = clock();

		PlayerControl *pc = &(getPlayerData()->playerControl);

		unblockSignals();

		setSigHandlersForDecoder();

		closeAllListenSockets();
		freeAllInterfaces();
		finishPlaylist();
		closeMp3Directory();
		finishPermissions();
		finishCommands();
		finishVolume();

		DEBUG("took %f to init player\n", 
		      (float)(clock()-start)/CLOCKS_PER_SEC);

		while (1) {
			if (pc->play)
				decode();
			else if (pc->stop)
				pc->stop = 0;
			else if (pc->seek)
				pc->seek = 0;
			else if (pc->pause)
				pc->pause = 0;
			else if (pc->closeAudio) {
				closeAudioDevice();
				pc->closeAudio = 0;
				kill(getppid(), SIGUSR1);
			} else if (pc->lockQueue) {
				pc->queueLockState = PLAYER_QUEUE_LOCKED;
				pc->lockQueue = 0;
			} else if (pc->unlockQueue) {
				pc->queueLockState = PLAYER_QUEUE_UNLOCKED;
				pc->unlockQueue = 0;
			} else if (pc->cycleLogFiles) {
				cycle_log_files();
				pc->cycleLogFiles = 0;
			} else
				my_usleep(10000);
		}

		exit(EXIT_SUCCESS);
	} 
	else if (player_pid < 0) 
	{
		unblockSignals();
		ERROR("player Problems fork()'ing\n");
		player_pid = 0;
		return -1;
	}

	unblockSignals();

	return 0;
}

int playerPlay(int fd, Song * song)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (playerStop(fd) < 0)
		return -1;

	if (song->tag)
		pc->fileTime = song->tag->time;
	else
		pc->fileTime = 0;

	copyMpdTagToMetadataChunk(song->tag, &(pc->fileMetadataChunk));

	pathcpy_trunc(pc->utf8url, getSongUrl(song));

	pc->play = 1;
	if (player_pid == 0 && playerInit() < 0) {
		pc->play = 0;
		return -1;
	}

	resetPlayerMetadata();
	while (player_pid > 0 && pc->play)
		my_usleep(1000);

	return 0;
}

int playerStop(int fd)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (player_pid > 0 && pc->state != PLAYER_STATE_STOP) {
		pc->stop = 1;
		while (player_pid > 0 && pc->stop)
			my_usleep(1000);
	}

	pc->queueState = PLAYER_QUEUE_BLANK;
	playerQueueUnlock();

	return 0;
}

void playerKill(void)
{
	int pid;
	/*PlayerControl * pc = &(getPlayerData()->playerControl);

	   playerStop(stderr);
	   playerCloseAudio(stderr);
	   if(player_pid>0 && pc->closeAudio) sleep(1); */

	pid = player_pid;
	if (pid > 0)
		kill(pid, SIGTERM);
}

int playerPause(int fd)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (player_pid > 0 && pc->state != PLAYER_STATE_STOP) {
		pc->pause = 1;
		while (player_pid > 0 && pc->pause)
			my_usleep(1000);
	}

	return 0;
}

int playerSetPause(int fd, int pause)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (player_pid <= 0)
		return 0;

	switch (pc->state) {
	case PLAYER_STATE_PLAY:
		if (pause)
			playerPause(fd);
		break;
	case PLAYER_STATE_PAUSE:
		if (!pause)
			playerPause(fd);
		break;
	}

	return 0;
}

int getPlayerElapsedTime(void)
{
	return (int)(getPlayerData()->playerControl.elapsedTime + 0.5);
}

unsigned long getPlayerBitRate(void)
{
	return getPlayerData()->playerControl.bitRate;
}

int getPlayerTotalTime(void)
{
	return (int)(getPlayerData()->playerControl.totalTime + 0.5);
}

int getPlayerState(void)
{
	return getPlayerData()->playerControl.state;
}

void clearPlayerError(void)
{
	getPlayerData()->playerControl.error = 0;
}

int getPlayerError(void)
{
	return getPlayerData()->playerControl.error;
}

char *getPlayerErrorStr(void)
{
	static char *error;
	int errorlen = MAXPATHLEN + 1024;
	PlayerControl *pc = &(getPlayerData()->playerControl);

	error = xrealloc(error, errorlen);
	error[0] = '\0';

	switch (pc->error) {
	case PLAYER_ERROR_FILENOTFOUND:
		snprintf(error, errorlen,
			 "file \"%s\" does not exist or is inaccessible",
			 pc->erroredUrl);
		break;
	case PLAYER_ERROR_FILE:
		snprintf(error, errorlen, "problems decoding \"%s\"",
			 pc->erroredUrl);
		break;
	case PLAYER_ERROR_AUDIO:
		snprintf(error, errorlen, "problems opening audio device");
		break;
	case PLAYER_ERROR_SYSTEM:
		snprintf(error, errorlen, "system error occured");
		break;
	case PLAYER_ERROR_UNKTYPE:
		snprintf(error, errorlen, "file type  of \"%s\" is unknown",
			 pc->erroredUrl);
	default:
		break;
	}

	errorlen = strlen(error);
	error = xrealloc(error, errorlen + 1);

	if (errorlen)
		return error;

	return NULL;
}

void playerCloseAudio(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (player_pid > 0) {
		if (playerStop(STDERR_FILENO) < 0)
			return;
		pc->closeAudio = 1;
	}
}

int queueSong(Song * song)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (pc->queueState == PLAYER_QUEUE_BLANK) {
		pathcpy_trunc(pc->utf8url, getSongUrl(song));

		if (song->tag)
			pc->fileTime = song->tag->time;
		else
			pc->fileTime = 0;

		copyMpdTagToMetadataChunk(song->tag, &(pc->fileMetadataChunk));

		pc->queueState = PLAYER_QUEUE_FULL;
		return 0;
	}

	return -1;
}

int getPlayerQueueState(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	return pc->queueState;
}

void setQueueState(int queueState)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	pc->queueState = queueState;
}

void playerQueueLock(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (player_pid > 0 && pc->queueLockState == PLAYER_QUEUE_UNLOCKED) {
		pc->lockQueue = 1;
		while (player_pid > 0 && pc->lockQueue)
			my_usleep(1000);
	}
}

void playerQueueUnlock(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (player_pid > 0 && pc->queueLockState == PLAYER_QUEUE_LOCKED) {
		pc->unlockQueue = 1;
		while (player_pid > 0 && pc->unlockQueue)
			my_usleep(1000);
	}
}

int playerSeek(int fd, Song * song, float time)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (pc->state == PLAYER_STATE_STOP) {
		commandError(fd, ACK_ERROR_PLAYER_SYNC,
			     "player not currently playing");
		return -1;
	}

	if (strcmp(pc->utf8url, getSongUrl(song)) != 0) {
		if (song->tag)
			pc->fileTime = song->tag->time;
		else
			pc->fileTime = 0;

		copyMpdTagToMetadataChunk(song->tag, &(pc->fileMetadataChunk));

		pathcpy_trunc(pc->utf8url, getSongUrl(song));
	}

	if (pc->error == PLAYER_ERROR_NOERROR) {
		resetPlayerMetadata();
		pc->seekWhere = time;
		pc->seek = 1;
		while (player_pid > 0 && pc->seek)
			my_usleep(1000);
	}

	return 0;
}

float getPlayerCrossFade(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	return pc->crossFade;
}

void setPlayerCrossFade(float crossFadeInSeconds)
{
	PlayerControl *pc;
	if (crossFadeInSeconds < 0)
		crossFadeInSeconds = 0;

	pc = &(getPlayerData()->playerControl);

	pc->crossFade = crossFadeInSeconds;
}

void setPlayerSoftwareVolume(int volume)
{
	PlayerControl *pc;
	volume = (volume > 1000) ? 1000 : (volume < 0 ? 0 : volume);

	pc = &(getPlayerData()->playerControl);

	pc->softwareVolume = volume;
}

double getPlayerTotalPlayTime(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	return pc->totalPlayTime;
}

unsigned int getPlayerSampleRate(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	return pc->sampleRate;
}

int getPlayerBits(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	return pc->bits;
}

int getPlayerChannels(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);

	return pc->channels;
}

void playerCycleLogFiles(void)
{
	PlayerControl *pc = &(getPlayerData()->playerControl);
	DecoderControl *dc = &(getPlayerData()->decoderControl);

	pc->cycleLogFiles = 1;
	dc->cycleLogFiles = 1;
}

/* this actually creates a dupe of the current metadata */
Song *playerCurrentDecodeSong(void)
{
	static Song *song;
	static MetadataChunk *prev;
	Song *ret = NULL;
	PlayerControl *pc = &(getPlayerData()->playerControl);

	if (pc->metadataState == PLAYER_METADATA_STATE_READ) {
		DEBUG("playerCurrentDecodeSong: caught new metadata!\n");
		if (prev)
			free(prev);
		prev = xmalloc(sizeof(MetadataChunk));
		memcpy(prev, &(pc->metadataChunk), sizeof(MetadataChunk));
		if (song)
			freeJustSong(song);
		song = newNullSong();
		song->url = xstrdup(pc->currentUrl);
		song->tag = metadataChunkToMpdTagDup(prev);
		ret = song;
		resetPlayerMetadata();
	}

	return ret;
}
