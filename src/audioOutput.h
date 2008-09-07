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

#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include "output_api.h"
#include "../config.h"

#include "conf.h"
#include "os_compat.h"

struct audio_format;
struct tag;

void initAudioOutputPlugins(void);
void finishAudioOutputPlugins(void);

void loadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin);
void unloadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin);

int initAudioOutput(AudioOutput *, ConfigParam * param);
int openAudioOutput(AudioOutput * audioOutput,
		    const struct audio_format *audioFormat);
int playAudioOutput(AudioOutput * audioOutput,
		    const char *playChunk, size_t size);
void dropBufferedAudioOutput(AudioOutput * audioOutput);
void closeAudioOutput(AudioOutput * audioOutput);
void finishAudioOutput(AudioOutput * audioOutput);
int keepAudioOutputAlive(AudioOutput * audioOutput, int ms);
void sendMetadataToAudioOutput(AudioOutput * audioOutput,
			       const struct tag *tag);

void printAllOutputPluginTypes(FILE * fp);

extern AudioOutputPlugin shoutPlugin;
extern AudioOutputPlugin nullPlugin;
extern AudioOutputPlugin fifoPlugin;
extern AudioOutputPlugin alsaPlugin;
extern AudioOutputPlugin aoPlugin;
extern AudioOutputPlugin ossPlugin;
extern AudioOutputPlugin osxPlugin;
extern AudioOutputPlugin pulsePlugin;
extern AudioOutputPlugin mvpPlugin;
extern AudioOutputPlugin jackPlugin;

#endif
