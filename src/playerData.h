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

#ifndef PLAYER_DATA_H
#define PLAYER_DATA_H

#include "audio.h"
#include "player.h"
#include "decode.h"
#include "mpd_types.h"

#define CHUNK_SIZE		1024

extern int buffered_before_play;
extern int buffered_chunks;

typedef struct _Buffer {
	char * chunks;
	mpd_uint16 * chunkSize;
	mpd_uint16 * bitRate;
	float * times;
	int begin;
	int end;
	int next;
	int wrap;
	float totalTime;
} Buffer;

typedef struct _PlayerData {
	Buffer buffer;
	AudioFormat audioFormat;
	PlayerControl playerControl;
	DecoderControl decoderControl;
} PlayerData;

void initPlayerData();

PlayerData * getPlayerData();
Buffer * getBuffer();

void freePlayerData();

#endif
