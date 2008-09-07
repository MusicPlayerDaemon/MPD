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
#include "output_api.h"

#include "list.h"
#include "log.h"
#include "pcm_utils.h"
#include "utils.h"
#include "os_compat.h"
#include "audio.h"

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"

static List *audioOutputPluginList;

void loadAudioOutputPlugin(struct audio_output_plugin *audioOutputPlugin)
{
	if (!audioOutputPlugin->name)
		return;
	insertInList(audioOutputPluginList, audioOutputPlugin->name,
		     audioOutputPlugin);
}

void unloadAudioOutputPlugin(struct audio_output_plugin *audioOutputPlugin)
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

int initAudioOutput(struct audio_output *ao, ConfigParam * param)
{
	void *data = NULL;
	const char *name = NULL;
	char *format = NULL;
	BlockParam *bp = NULL;
	struct audio_output_plugin *plugin = NULL;

	if (param) {
		const char *type = NULL;

		getBlockParam(AUDIO_OUTPUT_NAME, name, 1);
		getBlockParam(AUDIO_OUTPUT_TYPE, type, 1);
		getBlockParam(AUDIO_OUTPUT_FORMAT, format, 0);

		if (!findInList(audioOutputPluginList, type, &data)) {
			FATAL("couldn't find audio output plugin for type "
			      "\"%s\" at line %i\n", type, param->line);
		}

		plugin = (struct audio_output_plugin *) data;
	} else {
		ListNode *node = audioOutputPluginList->firstNode;

		WARNING("No \"%s\" defined in config file\n",
			CONF_AUDIO_OUTPUT);
		WARNING("Attempt to detect audio output device\n");

		while (node) {
			plugin = (struct audio_output_plugin *) node->data;
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
	}

	ao->name = name;
	ao->plugin = plugin;
	ao->open = 0;

	ao->convertAudioFormat = 0;
	ao->sameInAndOutFormats = 0;
	ao->convBuffer = NULL;
	ao->convBufferLen = 0;

	memset(&ao->inAudioFormat, 0, sizeof(ao->inAudioFormat));
	memset(&ao->outAudioFormat, 0, sizeof(ao->outAudioFormat));
	memset(&ao->reqAudioFormat, 0, sizeof(ao->reqAudioFormat));
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

int openAudioOutput(struct audio_output *audioOutput,
		    const struct audio_format *audioFormat)
{
	int ret = 0;

	if (audioOutput->open &&
	    0 == cmpAudioFormat(audioFormat, &audioOutput->inAudioFormat)) {
		return 0;
	}

	copyAudioFormat(&audioOutput->inAudioFormat, audioFormat);

	if (audioOutput->convertAudioFormat) {
		copyAudioFormat(&audioOutput->outAudioFormat,
		                &audioOutput->reqAudioFormat);
	} else {
		copyAudioFormat(&audioOutput->outAudioFormat,
		                &audioOutput->inAudioFormat);
		if (audioOutput->open)
			closeAudioOutput(audioOutput);
	}

	if (!audioOutput->open)
		ret = audioOutput->plugin->openDeviceFunc(audioOutput);

	audioOutput->sameInAndOutFormats =
		!cmpAudioFormat(&audioOutput->inAudioFormat,
		                &audioOutput->outAudioFormat);

	return ret;
}

static void convertAudioFormat(struct audio_output *audioOutput,
			       const char **chunkArgPtr, size_t *sizeArgPtr)
{
	size_t size = pcm_sizeOfConvBuffer(&(audioOutput->inAudioFormat),
					   *sizeArgPtr,
					   &(audioOutput->outAudioFormat));

	if (size > audioOutput->convBufferLen) {
		if (audioOutput->convBuffer != NULL)
			free(audioOutput->convBuffer);
		audioOutput->convBuffer = xmalloc(size);
		audioOutput->convBufferLen = size;
	}

	*sizeArgPtr = pcm_convertAudioFormat(&(audioOutput->inAudioFormat), 
	                                     *chunkArgPtr, *sizeArgPtr, 
	                                     &(audioOutput->outAudioFormat),
	                                     audioOutput->convBuffer,
	                                     &audioOutput->convState);

	*chunkArgPtr = audioOutput->convBuffer;
}

int playAudioOutput(struct audio_output *audioOutput,
		    const char *playChunk, size_t size)
{
	int ret;

	if (!audioOutput->open)
		return -1;

	if (!audioOutput->sameInAndOutFormats) {
		convertAudioFormat(audioOutput, &playChunk, &size);
	}

	ret = audioOutput->plugin->playFunc(audioOutput, playChunk, size);

	return ret;
}

void dropBufferedAudioOutput(struct audio_output *audioOutput)
{
	if (audioOutput->open)
		audioOutput->plugin->dropBufferedAudioFunc(audioOutput);
}

void closeAudioOutput(struct audio_output *audioOutput)
{
	if (audioOutput->open)
		audioOutput->plugin->closeDeviceFunc(audioOutput);
}

void finishAudioOutput(struct audio_output *audioOutput)
{
	closeAudioOutput(audioOutput);
	if (audioOutput->plugin->finishDriverFunc)
		audioOutput->plugin->finishDriverFunc(audioOutput);
	if (audioOutput->convBuffer)
		free(audioOutput->convBuffer);
}

void sendMetadataToAudioOutput(struct audio_output *audioOutput,
			       const struct tag *tag)
{
	if (!audioOutput->plugin->sendMetdataFunc)
		return;
	audioOutput->plugin->sendMetdataFunc(audioOutput, tag);
}

void printAllOutputPluginTypes(FILE * fp)
{
	ListNode *node = audioOutputPluginList->firstNode;
	struct audio_output_plugin *plugin;

	while (node) {
		plugin = (struct audio_output_plugin *) node->data;
		fprintf(fp, "%s ", plugin->name);
		node = node->nextNode;
	}
	fprintf(fp, "\n");
	fflush(fp);
}
