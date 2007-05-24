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

#include "pcm_utils.h"
#include "mpd_types.h"
#include "audio.h"
#include "tag.h"
#include "conf.h"
#include "utils.h"

#define DISABLED_AUDIO_OUTPUT_PLUGIN(plugin) AudioOutputPlugin plugin;

typedef struct _AudioOutput AudioOutput;

typedef int (*AudioOutputTestDefaultDeviceFunc) ();

typedef int (*AudioOutputInitDriverFunc) (AudioOutput * audioOutput,
					  ConfigParam * param);

typedef void (*AudioOutputFinishDriverFunc) (AudioOutput * audioOutput);

typedef int (*AudioOutputOpenDeviceFunc) (AudioOutput * audioOutput);

typedef int (*AudioOutputPlayFunc) (AudioOutput * audioOutput,
				    char *playChunk, int size);

typedef void (*AudioOutputDropBufferedAudioFunc) (AudioOutput * audioOutput);

typedef void (*AudioOutputCloseDeviceFunc) (AudioOutput * audioOutput);

typedef void (*AudioOutputSendMetadataFunc) (AudioOutput * audioOutput,
					     MpdTag * tag);

struct _AudioOutput {
	int open;
	char *name;
	char *type;

	AudioOutputFinishDriverFunc finishDriverFunc;
	AudioOutputOpenDeviceFunc openDeviceFunc;
	AudioOutputPlayFunc playFunc;
	AudioOutputDropBufferedAudioFunc dropBufferedAudioFunc;
	AudioOutputCloseDeviceFunc closeDeviceFunc;
	AudioOutputSendMetadataFunc sendMetdataFunc;

	int convertAudioFormat;
	AudioFormat inAudioFormat;
	AudioFormat outAudioFormat;
	AudioFormat reqAudioFormat;
	ConvState convState;
	char *convBuffer;
	int convBufferLen;
	int sameInAndOutFormats;

	void *data;
};

typedef struct _AudioOutputPlugin {
	char *name;

	AudioOutputTestDefaultDeviceFunc testDefaultDeviceFunc;
	AudioOutputInitDriverFunc initDriverFunc;
	AudioOutputFinishDriverFunc finishDriverFunc;
	AudioOutputOpenDeviceFunc openDeviceFunc;
	AudioOutputPlayFunc playFunc;
	AudioOutputDropBufferedAudioFunc dropBufferedAudioFunc;
	AudioOutputCloseDeviceFunc closeDeviceFunc;
	AudioOutputSendMetadataFunc sendMetdataFunc;
} AudioOutputPlugin;

void initAudioOutputPlugins(void);
void finishAudioOutputPlugins(void);

void loadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin);
void unloadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin);

int initAudioOutput(AudioOutput *, ConfigParam * param);
int openAudioOutput(AudioOutput * audioOutput, AudioFormat * audioFormat);
int playAudioOutput(AudioOutput * audioOutput, char *playChunk, int size);
void dropBufferedAudioOutput(AudioOutput * audioOutput);
void closeAudioOutput(AudioOutput * audioOutput);
void finishAudioOutput(AudioOutput * audioOutput);
int keepAudioOutputAlive(AudioOutput * audioOutput, int ms);
void sendMetadataToAudioOutput(AudioOutput * audioOutput, MpdTag * tag);

void printAllOutputPluginTypes(FILE * fp);

extern AudioOutputPlugin alsaPlugin;
extern AudioOutputPlugin aoPlugin;
extern AudioOutputPlugin ossPlugin;
extern AudioOutputPlugin osxPlugin;
extern AudioOutputPlugin pulsePlugin;
extern AudioOutputPlugin mvpPlugin;
extern AudioOutputPlugin shoutPlugin;
extern AudioOutputPlugin jackPlugin;

#endif
