#include "input_plugin.h"

#include <stdlib.h>

InputPlugin * newInputPlugin() {
	InputPlugin * ret = malloc(sizeof(InputPlugin));

	memset(ret->name,0,INPUT_PLUGIN_NAME_LENGTH);

	ret->suffixes = NULL;
	ret->mimeTypes = NULL;
	ret->streamTypes = 0;

	ret->streamDecodeFunc = NULL;
	ret->fileDeocdeFunc = NULL;
	ret->tagDupFunc = NULL;

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
	char * temp;

	freeStringArray(inPlugin->suffixes);
	freeStringArray(inPlugin->mimeTypes);

	free(inPlugin);
}

static char ** AddStringToArray(char ** array, char * string) {
	int arraySize = 1;

	if(array) {
		char ** tmp = array;
		while(*array) {
			arraySize++;
			array++;
		}
	}

	array = realloc(array, arraySize*sizeof(char *));

	array[arraySize-1] = strdup(string);

	return array;
}

void addSuffixToInputPlugin(InputPlugin * inPlugin, char * suffix) {
	inPlugin->suffixes = AddStringToArray(inPlugin->suffixes, suffix);
}

void addMimeTypeToInputPlugin(InputPlugin * inPlugin, char * mimeType) {
	inPlugin->mimeTypes = AddStringToArray(inPlugin->mimeTypes, mimeType);
}
