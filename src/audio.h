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

#ifndef MPD_AUDIO_H
#define MPD_AUDIO_H

#include <stdbool.h>

struct audio_format;

void getOutputAudioFormat(const struct audio_format *inFormat,
			  struct audio_format *outFormat);

/* make sure initPlayerData is called before this function!! */
void initAudioConfig(void);

void finishAudioConfig(void);

/* these functions are called in the main parent process while the child
	process is busy playing to the audio */
int enableAudioDevice(unsigned int device);

int disableAudioDevice(unsigned int device);

bool mixer_control_setvol(unsigned int device, int volume, int rel);
bool mixer_control_getvol(unsigned int device, int *volume);

#endif
