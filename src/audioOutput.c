#include <audioOutput.h>

#include <list.h>

static List * audioOutputPluginList;

void loadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin) {
	insertInList(audioOutputPluginList, audioOutputPlugin->name,
			audioOutputPlugin);
}

void unloadAudioOutputPlugin(AudioOutputPlugin * audioOutputPlugin) {
	deleteFromList(audioOutputPluginList, audioOutputPlugin->name);
}

void initAudioOutputPlugins() {
	audioOutputPluginList = makeList(NULL);
}

void finishAudioOutputPlugins() {
	freeList(audioOutputPluginList);
}

AudioOutput * newAudioOutput(char * name) {
	AudioOutput * ret = NULL;
	void * data = NULL;

	if(findInList(audioOutputPluginList, name, &data)) {
		AudioOutputPlugin * plugin = (AudioOutputPlugin *) data;
		ret = malloc(sizeof(AudioOutput));
		ret->finishDriverFunc = plugin->finishDriverFunc;
		ret->openDeviceFunc = plugin->openDeviceFunc;
		ret->playFunc = plugin->playFunc;
		ret->closeDeviceFunc = plugin->closeDeviceFunc;
		ret->sendMetdataFunc = plugin->sendMetdataFunc;
		ret->open = 0;

		if(plugin->initDriverFunc(ret) != 0) {
			free(ret);
			ret = NULL;
		}
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
	free(audioOutput);
}

void sendMetadataToAudioOutput(AudioOutput * audioOutput, MpdTag * tag) {
	if(!audioOutput->open || !audioOutput->sendMetdataFunc) return;
	audioOutput->sendMetdataFunc(audioOutput, tag);
}
