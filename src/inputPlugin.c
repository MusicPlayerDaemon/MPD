#include "inputPlugin.h"

#include "list.h"

#include <stdlib.h>
#include <string.h>

static List * inputPlugin_list = NULL;

void loadInputPlugin(InputPlugin * inputPlugin) {
	if(!inputPlugin) return;
	if(!inputPlugin->name) return;

	insertInList(inputPlugin_list, inputPlugin->name, (void *)inputPlugin);
}

void unloadInputPlugin(InputPlugin * inputPlugin) {
	deleteFromList(inputPlugin_list, inputPlugin->name);
}

static int stringFoundInStringArray(char ** array, char * suffix) {
	while(array && *array) {
		if(strcmp(*array, suffix) == 0) return 1;
		array++;
	}
	
	return 0;
}

InputPlugin * getInputPluginFromSuffix(char * suffix) {
	ListNode * node = inputPlugin_list->firstNode;
	InputPlugin * plugin = NULL;

	if(suffix == NULL) return NULL;

	while(node != NULL) {
		plugin = node->data;
		if(stringFoundInStringArray(plugin->suffixes, suffix)) {
			return plugin;
		}
		node = node->nextNode;
	}

	return NULL;
}

InputPlugin * getInputPluginFromMimeType(char * mimeType) {
	ListNode * node = inputPlugin_list->firstNode;
	InputPlugin * plugin = NULL;

	while(node != NULL) {
		plugin = node->data;
		if(stringFoundInStringArray(plugin->mimeTypes, mimeType)) {
			return plugin;
		}
		node = node->nextNode;
	}

	return NULL;
}

InputPlugin * getInputPluginFromName(char * name) {
	void * plugin = NULL;

	findInList(inputPlugin_list, name, &plugin);

	return (InputPlugin *)plugin;
}

extern InputPlugin mp3Plugin;
extern InputPlugin oggPlugin;

void initInputPlugins() {
	inputPlugin_list = makeList(NULL);

	/* load plugins here */
	loadInputPlugin(&mp3Plugin);
	loadInputPlugin(&oggPlugin);
}

void finishInputPlugins() {
	freeList(inputPlugin_list);
}
