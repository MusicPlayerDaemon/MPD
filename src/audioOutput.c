#include "audioOutput.h"

#include "list.h"
#include "log.h"
#include "pcm_utils.h"

#include <string.h> 

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"

static List * audioOutputPluginList;

void loadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin) {
	if(!audioOutputPlugin->name) return;
	insertInList(audioOutputPluginList, audioOutputPlugin->name,
			audioOutputPlugin);
}

void unloadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin) {
	if(!audioOutputPlugin->name) return;
	deleteFromList(audioOutputPluginList, audioOutputPlugin->name);
}

void initAudioOutputPlugins() {
	audioOutputPluginList = makeList(NULL, 0);
}

void finishAudioOutputPlugins() {
	freeList(audioOutputPluginList);
}

#define getBlockParam(name, str, force) { \
	bp = getBlockParam(param, name); \
	if(force && bp == NULL) { \
		ERROR("couldn't find parameter \"%s\" in audio output " \
				"definition begining at %i\n", \
				name, param->line); \
		exit(EXIT_FAILURE); \
	} \
	if(bp) str = bp->value; \
}

AudioOutput * newAudioOutput(ConfigParam * param) {
	AudioOutput * ret = NULL;
	void * data = NULL;
	char * name = NULL;
	char * format = NULL;
	char * type = NULL;
	BlockParam * bp;

	getBlockParam(AUDIO_OUTPUT_NAME, name, 1);
	getBlockParam(AUDIO_OUTPUT_TYPE, type, 1);

	if(findInList(audioOutputPluginList, type, &data)) {
		AudioOutputPlugin * plugin = (AudioOutputPlugin *) data;
		ret = malloc(sizeof(AudioOutput));
		ret->name = strdup(name);
		ret->type = strdup(type);
		ret->finishDriverFunc = plugin->finishDriverFunc;
		ret->openDeviceFunc = plugin->openDeviceFunc;
		ret->playFunc = plugin->playFunc;
		ret->dropBufferedAudioFunc = plugin->dropBufferedAudioFunc;
		ret->closeDeviceFunc = plugin->closeDeviceFunc;
		ret->sendMetdataFunc = plugin->sendMetdataFunc;
		ret->open = 0;

		ret->convertAudioFormat = 0;
		ret->sameInAndOutFormats = 0;
		ret->convBuffer = NULL;
		ret->convBufferLen = 0;

		memset(&ret->inAudioFormat, 0, sizeof(AudioFormat));
		memset(&ret->outAudioFormat, 0, sizeof(AudioFormat));

		getBlockParam(AUDIO_OUTPUT_FORMAT, format, 0);

		if(format) {
			ret->convertAudioFormat = 1;

			if(0 != parseAudioConfig(&ret->outAudioFormat, format))
			{
		        	ERROR("error parsing format at line %i\n", 
						bp->line);
		        	exit(EXIT_FAILURE);
		        }
		}

		if(plugin->initDriverFunc(ret, param) != 0) {
			free(ret);
			ret = NULL;
		}
	}
	else {
		ERROR("couldn't find audio output plugin for type \"%s\" at "
				"line %i\n", type, param->line);
		exit(EXIT_FAILURE);
	}

	return ret;
}

int openAudioOutput(AudioOutput * audioOutput, AudioFormat * audioFormat) {
	if(audioOutput->open) {
		if(cmpAudioFormat(audioFormat, &audioOutput->inAudioFormat) 
				== 0) 
		{
			return 0;
		}
		closeAudioOutput(audioOutput);
	}

	copyAudioFormat(&audioOutput->inAudioFormat, audioFormat);

	if(audioOutput->convertAudioFormat) {
		if(cmpAudioFormat(&audioOutput->inAudioFormat, 
				&audioOutput->outAudioFormat) == 0) 
		{
			audioOutput->sameInAndOutFormats = 1;
		}
		else audioOutput->sameInAndOutFormats = 0;
	}
	else {
		audioOutput->sameInAndOutFormats = 1;
		copyAudioFormat(&audioOutput->outAudioFormat, 
				&audioOutput->inAudioFormat);
	}

	return audioOutput->openDeviceFunc(audioOutput);
}

static void convertAudioFormat(AudioOutput * audioOutput, char ** chunkArgPtr,
		int * sizeArgPtr)
{
	int size = pcm_sizeOfOutputBufferForAudioFormatConversion(
			&(audioOutput->inAudioFormat), *sizeArgPtr, 
			&(audioOutput->outAudioFormat));

	if(size > audioOutput->convBufferLen) {
		audioOutput->convBuffer = 
				realloc(audioOutput->convBuffer, size);
		audioOutput->convBufferLen = size;
	}

	pcm_convertAudioFormat(&(audioOutput->inAudioFormat), *chunkArgPtr, 
			*sizeArgPtr, &(audioOutput->outAudioFormat), 
			audioOutput->convBuffer);
	
	*sizeArgPtr = size;
	*chunkArgPtr = audioOutput->convBuffer;
}

int playAudioOutput(AudioOutput * audioOutput, char * playChunk, int size) {
	int ret;

	if(!audioOutput->open) return -1;

	if(!audioOutput->sameInAndOutFormats) {
		convertAudioFormat(audioOutput, &playChunk, &size);
	}


	ret = audioOutput->playFunc(audioOutput, playChunk, size);

	return ret;
}

void dropBufferedAudioOutput(AudioOutput * audioOutput) {
	if(audioOutput->open) audioOutput->dropBufferedAudioFunc(audioOutput);
}

void closeAudioOutput(AudioOutput * audioOutput) {
	if(audioOutput->open) audioOutput->closeDeviceFunc(audioOutput);
}

void finishAudioOutput(AudioOutput * audioOutput) {
	closeAudioOutput(audioOutput);
	audioOutput->finishDriverFunc(audioOutput);
	if(audioOutput->convBuffer) free(audioOutput->convBuffer);
	free(audioOutput->type);
	free(audioOutput->name);
	free(audioOutput);
}

void sendMetadataToAudioOutput(AudioOutput * audioOutput, MpdTag * tag) {
	if(!audioOutput->sendMetdataFunc) return;
	audioOutput->sendMetdataFunc(audioOutput, tag);
}
