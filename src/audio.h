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

#ifndef AUDIO_H
#define AUDIO_H

#include "../config.h"

#include "mpd_types.h"

#include <stdio.h>
#include <ao/ao.h>

#define AUDIO_AO_DRIVER_DEFAULT	"default"

typedef struct _AudioFormat {
	mpd_sint8 channels;
	mpd_uint32 sampleRate;
	mpd_sint8 bits;
} AudioFormat;

extern int audio_ao_driver_id;
extern ao_option * audio_ao_options;

void initAudioDriver();

void finishAudioDriver();

int initAudio(AudioFormat * audioFormat);

int playAudio(char * playChunk,int size);

void finishAudio();

void audioError();

int isCurrentAudioFormat(AudioFormat * audioFormat);

#endif
