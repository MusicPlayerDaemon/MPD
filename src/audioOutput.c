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
		ret->initConfigFunc = plugin->initConfigFunc;
		ret->finishConfigFunc = plugin->finishConfigFunc;
		ret->initDriverFunc = plugin->initDriverFunc;
		ret->finishDriverFunc = plugin->initDriverFunc;
		ret->openDeviceFunc = plugin->openDeviceFunc;
		ret->playFunc = plugin->playFunc;
		ret->closeDeviceFunc = plugin->closeDeviceFunc;
	}

	return ret;
}

void closeAudioOutput(AudioOutput * audioOutput);
