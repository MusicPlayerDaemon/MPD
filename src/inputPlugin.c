#include "inputPlugin.h"

#include "list.h"

#include <stdlib.h>
#include <string.h>

static List * inputPlugin_list = NULL;

InputPlugin * newInputPlugin(char * name, InputPlugin_streamDecodeFunc 
		streamDecodeFunc, InputPlugin_fileDecodeFunc fileDecodeFunc,
		InputPlugin_tagDupFunc tagDupFunc, unsigned char streamTypes)
{
	InputPlugin * ret = malloc(sizeof(InputPlugin));

	memset(ret->name,0,INPUT_PLUGIN_NAME_LENGTH);
	strncpy(ret->name, name, INPUT_PLUGIN_NAME_LENGTH-1);

	ret->suffixes = NULL;
	ret->mimeTypes = NULL;

	ret->streamTypes = streamTypes;

	ret->streamDecodeFunc = streamDecodeFunc;
	ret->fileDecodeFunc = fileDecodeFunc;
	ret->tagDupFunc = tagDupFunc;

	return ret;
}

static void freeStringArray(char ** ptr) {
	if(ptr) {
		char ** tmp = ptr;

		while(*tmp) {
			if(*tmp) free(*tmp);
			tmp++;
		}

		free (ptr);
	}
}

void freeInputPlugin(InputPlugin * inPlugin) {
	freeStringArray(inPlugin->suffixes);
	freeStringArray(inPlugin->mimeTypes);

	free(inPlugin);
}

static char ** AddStringToArray(char ** array, char * string) {
	int arraySize = 1;

	if(array) {
		char ** tmp = array;
		while(*tmp) {
			arraySize++;
			tmp++;
		}
	}

	array = realloc(array, (arraySize+1)*sizeof(char *));

	array[arraySize-1] = strdup(string);
	array[arraySize] = NULL;

	return array;
}

void addSuffixToInputPlugin(InputPlugin * inPlugin, char * suffix) {
	inPlugin->suffixes = AddStringToArray(inPlugin->suffixes, suffix);
}

void addMimeTypeToInputPlugin(InputPlugin * inPlugin, char * mimeType) {
	inPlugin->mimeTypes = AddStringToArray(inPlugin->mimeTypes, mimeType);
}

void loadInputPlugin(InputPlugin * inputPlugin) {
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

	while(node != NULL) {
		plugin = node->data;
		if(stringFoundInStringArray(plugin->suffixes, suffix)) {
			return plugin;
		}
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
	}

	return NULL;
}

InputPlugin * getInputPluginFromName(char * name) {
	void * plugin = NULL;

	findInList(inputPlugin_list, name, &plugin);

	return (InputPlugin *)plugin;
}

void initInputPlugins() {
	inputPlugin_list = makeList((ListFreeDataFunc *)freeInputPlugin);

	/* load plugins here */
}

void finishInputPlugins() {
	freeList(inputPlugin_list);
}
