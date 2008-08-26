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

enum player_state {
	PLAYER_STATE_STOP = 0,
	PLAYER_STATE_PAUSE,
	PLAYER_STATE_PLAY
};

enum player_command {
	PLAYER_COMMAND_NONE = 0,
	PLAYER_COMMAND_EXIT,
	PLAYER_COMMAND_STOP,
	PLAYER_COMMAND_PLAY,
	PLAYER_COMMAND_PAUSE,
	PLAYER_COMMAND_SEEK,
	PLAYER_COMMAND_CLOSE_AUDIO,
	PLAYER_COMMAND_LOCK_QUEUE,
	PLAYER_COMMAND_UNLOCK_QUEUE
};

#define PLAYER_ERROR_NOERROR		0
#define PLAYER_ERROR_FILE		1
#define PLAYER_ERROR_AUDIO		2
#define PLAYER_ERROR_SYSTEM		3
#define PLAYER_ERROR_UNKTYPE		4
#define PLAYER_ERROR_FILENOTFOUND	5

/* 0->1->2->3->5 regular playback
 *        ->4->0 don't play queued song
 */
enum player_queue_state {
	/** there is no queued song */
	PLAYER_QUEUE_BLANK = 0,

	/** there is a queued song */
	PLAYER_QUEUE_FULL = 1,

	/** the player thread has forwarded the queued song to the
	    decoder; it waits for PLAY or STOP */
	PLAYER_QUEUE_DECODE = 2,

	/** tells the player thread to start playing the queued song;
	    this is a response to DECODE */
	PLAYER_QUEUE_PLAY = 3,

	/** tells the player thread to stop before playing the queued
	    song; this is a response to DECODE */
	PLAYER_QUEUE_STOP = 4,

	/** the player thread has begun playing the queued song, and
	    thus its queue is empty */
	PLAYER_QUEUE_EMPTY = 5
};

#define PLAYER_QUEUE_UNLOCKED	0
#define PLAYER_QUEUE_LOCKED	1

typedef struct _PlayerControl {
	Notify notify;
	volatile enum player_command command;
	volatile enum player_state state;
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
	volatile enum player_queue_state queueState;
	volatile mpd_sint8 queueLockState;
	volatile double seekWhere;
	volatile float crossFade;
	volatile mpd_uint16 softwareVolume;
	volatile double totalPlayTime;
} PlayerControl;

void player_command_finished(void);

void playerPlay(Song * song);

void playerSetPause(int pause_flag);

void playerPause(void);

void playerKill(void);

int getPlayerTotalTime(void);

int getPlayerElapsedTime(void);

unsigned long getPlayerBitRate(void);

enum player_state getPlayerState(void);

void clearPlayerError(void);

char *getPlayerErrorStr(void);

int getPlayerError(void);

void playerWait(void);

void queueSong(Song * song);

enum player_queue_state getPlayerQueueState(void);

void setQueueState(enum player_queue_state queueState);

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
