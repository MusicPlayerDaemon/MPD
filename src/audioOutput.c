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
		ret->finishDriverFunc = plugin->initDriverFunc;
		ret->openDeviceFunc = plugin->openDeviceFunc;
		ret->playFunc = plugin->playFunc;
		ret->closeDeviceFunc = plugin->closeDeviceFunc;

		plugin->initDriverFunc(ret);
	}

	return ret;
}

int openAudioOutput(AudioOutput * audioOutput, AudioFormat * audioFormat) {
	return audioOutput->openDeviceFunc(audioOutput, audioFormat);
}

int playAudioOutput(AudioOutput * audioOutput, char * playChunk, int size) {
	return audioOutput->playFunc(audioOutput, playChunk, size);
}

void closeAudioOutput(AudioOutput * audioOutput) {
	audioOutput->closeDeviceFunc(audioOutput);
}

void finishAudioOutput(AudioOutput * audioOutput) {
	audioOutput->finishDriverFunc(audioOutput);
	free(audioOutput);
}
