#ifndef INPUT_PLUGIN_H
#define INPUT_PLUGIN_H

#include "../config.h"
#include "inputStream.h"
#include "decode.h"
#include "outputBuffer.h"
#include "tag.h"

#define INPUT_PLUGIN_STREAM_FILE	0x01
#define INPUT_PLUGIN_STREAM_URL		0x02

typedef int (* InputPlugin_initFunc) ();

typedef void (* InputPlugin_finishFunc) ();

typedef int (* InputPlugin_streamDecodeFunc) (OutputBuffer *, DecoderControl *,
		InputStream *);

typedef int (* InputPlugin_fileDecodeFunc) (OutputBuffer *, DecoderControl *,
		char * path);

/* file should be the full path! */
typedef MpdTag * (* InputPlugin_tagDupFunc) (char * file);

typedef struct _InputPlugin {
	char * name;
	InputPlugin_initFunc initFunc;
	InputPlugin_finishFunc finishFunc;
	InputPlugin_streamDecodeFunc streamDecodeFunc;
	InputPlugin_fileDecodeFunc fileDecodeFunc;
	InputPlugin_tagDupFunc tagDupFunc;
	unsigned char streamTypes;
	char ** suffixes;
	char ** mimeTypes;
} InputPlugin;

/* individual functions to load/unload plugins */
void loadInputPlugin(InputPlugin * inputPlugin);
void unloadInputPlugin(InputPlugin * inputPlugin);

/* interface for using plugins */

InputPlugin * getInputPluginFromSuffix(char * suffix);

InputPlugin * getInputPluginFromMimeType(char * mimeType);

InputPlugin * getInputPluginFromName(char * name);

void printAllInputPluginSuffixes(FILE * fp);

/* this is where we "load" all the "plugins" ;-) */
void initInputPlugins();

/* this is where we "unload" all the "plugins" */
void finishInputPlugins();

#endif
