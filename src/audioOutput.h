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

#include "../config.h"

#include "conf.h"
#include "os_compat.h"

struct audio_output;
struct audio_output_plugin;
struct audio_format;
struct tag;

void initAudioOutputPlugins(void);
void finishAudioOutputPlugins(void);

void loadAudioOutputPlugin(struct audio_output_plugin *audioOutputPlugin);
void unloadAudioOutputPlugin(struct audio_output_plugin *audioOutputPlugin);

int initAudioOutput(struct audio_output *, ConfigParam * param);
int openAudioOutput(struct audio_output *audioOutput,
		    const struct audio_format *audioFormat);
int playAudioOutput(struct audio_output *audioOutput,
		    const char *playChunk, size_t size);
void dropBufferedAudioOutput(struct audio_output *audioOutput);
void closeAudioOutput(struct audio_output *audioOutput);
void finishAudioOutput(struct audio_output *audioOutput);
int keepAudioOutputAlive(struct audio_output *audioOutput, int ms);
void sendMetadataToAudioOutput(struct audio_output *audioOutput,
			       const struct tag *tag);

void printAllOutputPluginTypes(FILE * fp);

extern struct audio_output_plugin shoutPlugin;
extern struct audio_output_plugin nullPlugin;
extern struct audio_output_plugin fifoPlugin;
extern struct audio_output_plugin alsaPlugin;
extern struct audio_output_plugin aoPlugin;
extern struct audio_output_plugin ossPlugin;
extern struct audio_output_plugin osxPlugin;
extern struct audio_output_plugin pulsePlugin;
extern struct audio_output_plugin mvpPlugin;
extern struct audio_output_plugin jackPlugin;

#endif
