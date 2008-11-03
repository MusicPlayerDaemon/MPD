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

#ifndef MPD_PLAYER_H
#define MPD_PLAYER_H

#include "notify.h"
#include "audio_format.h"

#include <stdint.h>

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

	/** player_control.next_song has been updated */
	PLAYER_COMMAND_QUEUE,

	/**
	 * cancel pre-decoding player_control.next_song; if the player
	 * has already started playing this song, it will completely
	 * stop
	 */
	PLAYER_COMMAND_CANCEL,
};

#define PLAYER_ERROR_NOERROR		0
#define PLAYER_ERROR_FILE		1
#define PLAYER_ERROR_AUDIO		2
#define PLAYER_ERROR_SYSTEM		3
#define PLAYER_ERROR_UNKTYPE		4
#define PLAYER_ERROR_FILENOTFOUND	5

struct player_control {
	unsigned int buffered_before_play;

	struct notify notify;
	volatile enum player_command command;
	volatile enum player_state state;
	volatile int8_t error;
	uint16_t bit_rate;
	struct audio_format audio_format;
	float total_time;
	float elapsed_time;
	struct song *volatile next_song;
	struct song *errored_song;
	volatile double seek_where;
	float cross_fade_seconds;
	uint16_t software_volume;
	double total_play_time;
};

extern struct player_control pc;

void pc_init(unsigned int buffered_before_play);

void pc_deinit(void);

void
playerPlay(struct song *song);

/**
 * see PLAYER_COMMAND_CANCEL
 */
void pc_cancel(void);

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

void
queueSong(struct song *song);

int
playerSeek(struct song *song, float seek_time);

void setPlayerCrossFade(float crossFadeInSeconds);

float getPlayerCrossFade(void);

void setPlayerSoftwareVolume(int volume);

double getPlayerTotalPlayTime(void);

static inline const struct audio_format *
player_get_audio_format(void)
{
	return &pc.audio_format;
}

struct song *
playerCurrentDecodeSong(void);

void playerInit(void);

#endif
