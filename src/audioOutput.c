#include "audioOutput.h"

#include "list.h"
#include "log.h"

#include <string.h> 

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"

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
	audioOutputPluginList = makeList(NULL);
}

void finishAudioOutputPlugins() {
	freeList(audioOutputPluginList);
}

#define getBlockParam(name, str) { \
	BlockParam * bp; \
	bp = getBlockParam(param, name); \
	if(bp == NULL) { \
		ERROR("couldn't find parameter \"%s\" in audio output " \
				"definition begining at %i\n", \
				name, param->line); \
		exit(EXIT_FAILURE); \
	} \
	str = bp->value; \
}

AudioOutput * newAudioOutput(ConfigParam * param) {
	AudioOutput * ret = NULL;
	void * data = NULL;
	char * name = NULL;
	char * type = NULL;

	getBlockParam(AUDIO_OUTPUT_NAME, name);
	getBlockParam(AUDIO_OUTPUT_TYPE, type);

	if(findInList(audioOutputPluginList, type, &data)) {
		AudioOutputPlugin * plugin = (AudioOutputPlugin *) data;
		ret = malloc(sizeof(AudioOutput));
		ret->name = strdup(name);
		ret->type = strdup(type);
		ret->finishDriverFunc = plugin->finishDriverFunc;
		ret->openDeviceFunc = plugin->openDeviceFunc;
		ret->playFunc = plugin->playFunc;
		ret->closeDeviceFunc = plugin->closeDeviceFunc;
		ret->sendMetdataFunc = plugin->sendMetdataFunc;
		ret->open = 0;

		if(plugin->initDriverFunc(ret, param) != 0) {
			free(ret);
			ret = NULL;
		}
	}
	else {
		ERROR("couldn't find audio output plugin for type \"%s\" at "
				"line %i", type, param->line);
		exit(EXIT_FAILURE);
	}

	return ret;
}

int openAudioOutput(AudioOutput * audioOutput, AudioFormat * audioFormat) {
	if(audioOutput->open) closeAudioOutput(audioOutput);
	return audioOutput->openDeviceFunc(audioOutput, audioFormat);
}

int playAudioOutput(AudioOutput * audioOutput, char * playChunk, int size) {
	if(!audioOutput->open) return -1;
	return audioOutput->playFunc(audioOutput, playChunk, size);
}

void closeAudioOutput(AudioOutput * audioOutput) {
	if(audioOutput->open) audioOutput->closeDeviceFunc(audioOutput);
}

void finishAudioOutput(AudioOutput * audioOutput) {
	closeAudioOutput(audioOutput);
	audioOutput->finishDriverFunc(audioOutput);
	free(audioOutput->type);
	free(audioOutput->name);
	free(audioOutput);
}

void sendMetadataToAudioOutput(AudioOutput * audioOutput, MpdTag * tag) {
	if(!audioOutput->sendMetdataFunc) return;
	audioOutput->sendMetdataFunc(audioOutput, tag);
}
