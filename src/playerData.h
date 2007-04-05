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

#ifndef PLAYER_DATA_H
#define PLAYER_DATA_H

#include "../config.h"

#include "audio.h"
#include "player.h"
#include "decode.h"
#include "mpd_types.h"
#include "outputBuffer.h"

/* pick 1020 since its devisible for 8,16,24, and 32-bit audio */
#define CHUNK_SIZE		1020

extern int buffered_before_play;
extern int buffered_chunks;

typedef struct _PlayerData {
	OutputBuffer buffer;
	PlayerControl playerControl;
	DecoderControl decoderControl;
	mpd_uint8 *audioDeviceStates;
} PlayerData;

void initPlayerData(void);

PlayerData *getPlayerData(void);

void freePlayerData(void);

#endif
