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

#include "audioOutput.h"

#include "list.h"
#include "log.h"
#include "pcm_utils.h"

#include <string.h>

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"

static List *audioOutputPluginList;

void loadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin)
{
	if (!audioOutputPlugin->name)
		return;
	insertInList(audioOutputPluginList, audioOutputPlugin->name,
		     audioOutputPlugin);
}

void unloadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin)
{
	if (!audioOutputPlugin->name)
		return;
	deleteFromList(audioOutputPluginList, audioOutputPlugin->name);
}

void initAudioOutputPlugins(void)
{
	audioOutputPluginList = makeList(NULL, 0);
}

void finishAudioOutputPlugins(void)
{
	freeList(audioOutputPluginList);
}

#define getBlockParam(name, str, force) { \
	bp = getBlockParam(param, name); \
	if(force && bp == NULL) { \
		FATAL("couldn't find parameter \"%s\" in audio output " \
				"definition beginning at %i\n", \
				name, param->line); \
	} \
	if(bp) str = bp->value; \
}

int initAudioOutput(AudioOutput *ao, ConfigParam * param)
{
	void *data = NULL;
	char *name = NULL;
	char *format = NULL;
	char *type = NULL;
	BlockParam *bp = NULL;
	AudioOutputPlugin *plugin = NULL;

	if (param) {
		getBlockParam(AUDIO_OUTPUT_NAME, name, 1);
		getBlockParam(AUDIO_OUTPUT_TYPE, type, 1);
		getBlockParam(AUDIO_OUTPUT_FORMAT, format, 0);

		if (!findInList(audioOutputPluginList, type, &data)) {
			FATAL("couldn't find audio output plugin for type "
			      "\"%s\" at line %i\n", type, param->line);
		}

		plugin = (AudioOutputPlugin *) data;
	} else {
		ListNode *node = audioOutputPluginList->firstNode;

		WARNING("No \"%s\" defined in config file\n",
			CONF_AUDIO_OUTPUT);
		WARNING("Attempt to detect audio output device\n");

		while (node) {
			plugin = (AudioOutputPlugin *) node->data;
			if (plugin->testDefaultDeviceFunc) {
				WARNING("Attempting to detect a %s audio "
					"device\n", plugin->name);
				if (plugin->testDefaultDeviceFunc() == 0) {
					WARNING("Successfully detected a %s "
						"audio device\n", plugin->name);
					break;
				}
			}
			node = node->nextNode;
		}

		if (!node) {
			WARNING("Unable to detect an audio device\n");
			return 0;
		}

		name = "default detected output";
		type = plugin->name;
	}

	ao->name = name;
	ao->type = type;
	ao->finishDriverFunc = plugin->finishDriverFunc;
	ao->openDeviceFunc = plugin->openDeviceFunc;
	ao->playFunc = plugin->playFunc;
	ao->dropBufferedAudioFunc = plugin->dropBufferedAudioFunc;
	ao->closeDeviceFunc = plugin->closeDeviceFunc;
	ao->sendMetdataFunc = plugin->sendMetdataFunc;
	ao->open = 0;

	ao->convertAudioFormat = 0;
	ao->sameInAndOutFormats = 0;
	ao->convBuffer = NULL;
	ao->convBufferLen = 0;

	memset(&ao->inAudioFormat, 0, sizeof(AudioFormat));
	memset(&ao->outAudioFormat, 0, sizeof(AudioFormat));
	memset(&ao->reqAudioFormat, 0, sizeof(AudioFormat));
	memset(&ao->convState, 0, sizeof(ConvState));

	if (format) {
		ao->convertAudioFormat = 1;

		if (0 != parseAudioConfig(&ao->reqAudioFormat, format)) {
			FATAL("error parsing format at line %i\n", bp->line);
		}

		copyAudioFormat(&ao->outAudioFormat, &ao->reqAudioFormat);
	}

	if (plugin->initDriverFunc(ao, param) != 0)
		return 0;

	return 1;
}

int openAudioOutput(AudioOutput * audioOutput, AudioFormat * audioFormat)
{
	int ret = 0;

	if (audioOutput->open) 
	{
		if (0==cmpAudioFormat(audioFormat, &audioOutput->inAudioFormat))
		{
			return 0;
		}
	}

	copyAudioFormat(&audioOutput->inAudioFormat, audioFormat);

	if (audioOutput->convertAudioFormat) 
	{
		copyAudioFormat(&audioOutput->outAudioFormat,
				&audioOutput->reqAudioFormat);
	} 
	else 
	{
		copyAudioFormat(&audioOutput->outAudioFormat,
				&audioOutput->inAudioFormat);
		if (audioOutput->open) closeAudioOutput(audioOutput);
	}

	if (!audioOutput->open)
	{
		ret = audioOutput->openDeviceFunc(audioOutput);
	}

	audioOutput->sameInAndOutFormats =
		!cmpAudioFormat(&audioOutput->inAudioFormat,
			   	&audioOutput->outAudioFormat);

	return ret;
}

static void convertAudioFormat(AudioOutput * audioOutput, char **chunkArgPtr,
			       int *sizeArgPtr)
{
	int size = pcm_sizeOfConvBuffer(&(audioOutput->inAudioFormat),
                                        *sizeArgPtr,
                                        &(audioOutput->outAudioFormat));

	if (size > audioOutput->convBufferLen) {
		audioOutput->convBuffer =
		    xrealloc(audioOutput->convBuffer, size);
		audioOutput->convBufferLen = size;
	}

	*sizeArgPtr = pcm_convertAudioFormat(&(audioOutput->inAudioFormat), 
	                                     *chunkArgPtr, *sizeArgPtr, 
	                                     &(audioOutput->outAudioFormat),
	                                     audioOutput->convBuffer,
	                                     &audioOutput->convState);

	*chunkArgPtr = audioOutput->convBuffer;
}

int playAudioOutput(AudioOutput * audioOutput, char *playChunk, int size)
{
	int ret;

	if (!audioOutput->open)
		return -1;

	if (!audioOutput->sameInAndOutFormats) {
		convertAudioFormat(audioOutput, &playChunk, &size);
	}

	ret = audioOutput->playFunc(audioOutput, playChunk, size);

	return ret;
}

void dropBufferedAudioOutput(AudioOutput * audioOutput)
{
	if (audioOutput->open)
		audioOutput->dropBufferedAudioFunc(audioOutput);
}

void closeAudioOutput(AudioOutput * audioOutput)
{
	if (audioOutput->open)
		audioOutput->closeDeviceFunc(audioOutput);
}

void finishAudioOutput(AudioOutput * audioOutput)
{
	closeAudioOutput(audioOutput);
	audioOutput->finishDriverFunc(audioOutput);
	if (audioOutput->convBuffer)
		free(audioOutput->convBuffer);
}

void sendMetadataToAudioOutput(AudioOutput * audioOutput, MpdTag * tag)
{
	if (!audioOutput->sendMetdataFunc)
		return;
	audioOutput->sendMetdataFunc(audioOutput, tag);
}

void printAllOutputPluginTypes(FILE * fp)
{
	ListNode *node = audioOutputPluginList->firstNode;
	AudioOutputPlugin *plugin;

	while (node) {
		plugin = (AudioOutputPlugin *) node->data;
		fprintf(fp, "%s ", plugin->name);
		node = node->nextNode;
	}
	fprintf(fp, "\n");
	fflush(fp);
}
