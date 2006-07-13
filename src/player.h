/* the Music Player Daemon (MPD)
 * (c)2003-2006 by Warren Dukes (shank@mercury.chem.pitt.edu)
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

#ifndef PLAYER_H
#define PLAYER_H

#include "../config.h"

#include "decode.h"
#include "mpd_types.h"
#include "song.h"
#include "metadataChunk.h"

#include <stdio.h>
#include <sys/param.h>

#define PLAYER_STATE_STOP	0
#define PLAYER_STATE_PAUSE	1
#define PLAYER_STATE_PLAY	2

#define PLAYER_ERROR_NOERROR		0
#define PLAYER_ERROR_FILE		1
#define PLAYER_ERROR_AUDIO		2
#define PLAYER_ERROR_SYSTEM		3
#define PLAYER_ERROR_UNKTYPE		4
#define PLAYER_ERROR_FILENOTFOUND	5

/* 0->1->2->3->5 regular playback
 *        ->4->0 don't play queued song
 */
#define PLAYER_QUEUE_BLANK	0
#define PLAYER_QUEUE_FULL	1
#define PLAYER_QUEUE_DECODE	2
#define PLAYER_QUEUE_PLAY	3
#define PLAYER_QUEUE_STOP	4
#define PLAYER_QUEUE_EMPTY	5

#define PLAYER_QUEUE_UNLOCKED	0
#define PLAYER_QUEUE_LOCKED	1

#define PLAYER_METADATA_STATE_READ      1
#define PLAYER_METADATA_STATE_WRITE     2

typedef struct _PlayerControl {
	volatile mpd_sint8 stop;
	volatile mpd_sint8 play;
	volatile mpd_sint8 pause;
	volatile mpd_sint8 state;
	volatile mpd_sint8 closeAudio;
	volatile mpd_sint8 error;
	volatile mpd_uint16 bitRate;
	volatile mpd_sint8 bits;
	volatile mpd_sint8 channels;
	volatile mpd_uint32 sampleRate;
	volatile float totalTime;
	volatile float elapsedTime;
        volatile float fileTime;
	char utf8url[MAXPATHLEN+1];
	char currentUrl[MAXPATHLEN+1];
	char erroredUrl[MAXPATHLEN+1];
	volatile mpd_sint8 queueState;
	volatile mpd_sint8 queueLockState;
	volatile mpd_sint8 lockQueue;
	volatile mpd_sint8 unlockQueue;
	volatile mpd_sint8 seek;
	volatile double seekWhere;
	volatile float crossFade;
	volatile mpd_uint16 softwareVolume;
	volatile double totalPlayTime;
	volatile int decode_pid;
	volatile mpd_sint8 cycleLogFiles;
        volatile mpd_sint8 metadataState;
        MetadataChunk metadataChunk;
        MetadataChunk fileMetadataChunk;
} PlayerControl;

void clearPlayerPid();

void player_sigChldHandler(int pid, int status);

int playerPlay(FILE * fp, Song * song);

int playerSetPause(FILE * fp, int pause);

int playerPause(FILE * fp);

int playerStop(FILE * fp);

void playerCloseAudio();

void playerKill();

void playerProcessMessages();

int getPlayerTotalTime();

int getPlayerElapsedTime();

unsigned long getPlayerBitRate();

int getPlayerState();

void clearPlayerError();

char * getPlayerErrorStr();

int getPlayerError();

int playerInit();

int queueSong(Song * song);

int getPlayerQueueState();

void setQueueState(int queueState);

void playerQueueLock();

void playerQueueUnlock();

int playerSeek(FILE * fp, Song * song, float time);

void setPlayerCrossFade(float crossFadeInSeconds);

float getPlayerCrossFade();

int getPlayerSoftwareVolume();

void setPlayerSoftwareVolume(int volume);

double getPlayerTotalPlayTime();

unsigned int getPlayerSampleRate();

int getPlayerBits();

int getPlayerChannels();

void playerCycleLogFiles();

Song * playerCurrentDecodeSong();

#endif
