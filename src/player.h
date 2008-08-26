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

#ifndef PLAYER_H
#define PLAYER_H

#include "notify.h"
#include "mpd_types.h"
#include "song.h"
#include "os_compat.h"

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

typedef struct _PlayerControl {
	Notify notify;
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
	Song *volatile next_song;
	Song *errored_song;
	volatile mpd_sint8 queueState;
	volatile mpd_sint8 queueLockState;
	volatile mpd_sint8 lockQueue;
	volatile mpd_sint8 unlockQueue;
	volatile mpd_sint8 seek;
	volatile double seekWhere;
	volatile float crossFade;
	volatile mpd_uint16 softwareVolume;
	volatile double totalPlayTime;
} PlayerControl;

void wakeup_player_nb(void);

void player_sleep(void);

int playerPlay(int fd, Song * song);

int playerSetPause(int fd, int pause_flag);

int playerPause(int fd);

int playerStop(int fd);

void playerKill(void);

int getPlayerTotalTime(void);

int getPlayerElapsedTime(void);

unsigned long getPlayerBitRate(void);

int getPlayerState(void);

void clearPlayerError(void);

char *getPlayerErrorStr(void);

int getPlayerError(void);

int playerWait(int fd);

int queueSong(Song * song);

int getPlayerQueueState(void);

void setQueueState(int queueState);

void playerQueueLock(void);

void playerQueueUnlock(void);

int playerSeek(int fd, Song * song, float seek_time);

void setPlayerCrossFade(float crossFadeInSeconds);

float getPlayerCrossFade(void);

void setPlayerSoftwareVolume(int volume);

double getPlayerTotalPlayTime(void);

unsigned int getPlayerSampleRate(void);

int getPlayerBits(void);

int getPlayerChannels(void);

Song *playerCurrentDecodeSong(void);

void playerInit(void);

#endif
