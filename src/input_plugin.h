#ifndef INPUT_PLUGIN_H
#define INPUT_PLUGIN_H

#include "inputStream.h"
#include "decode.h"
#include "outputBuffer.h"
#include "tag.h"

#define INPUT_PLUGIN_STREAM_FILE	0x01
#define INPUT_PLUGIN_STREAM_URL		0x02

#define INPUT_PLUGIN_NAME_LENGTH	64

typedef int (* InputPlugin_streamDecodeFunc) (OutputBuffer *, DecoderControl *,
		InputStream *);

typedef int (* InputPlugin_fileDecodeFunc) (OutputBuffer *, DecoderControl *);

typedef MpdTag * (* InputPlugin_tagDupFunc) (char * utf8file);

typedef struct _InputPlugin {
	char name[INPUT_PLUGIN_NAME_LENGTH];
	InputPlugin_streamDecodeFunc streamDecodeFunc;
	InputPlugin_fileDecodeFunc fileDecodeFunc;
	InputPlugin_tagDupFunc tagDupFunc;
	unsigned char streamTypes;
	char ** suffixes;
	char ** mimeTypes;
} InputPlugin;

/* interface for adding and removing plugins */

InputPlugin * newInputPlugin();

void addSuffixToInputPlugin(InputPlugin * inPlugin, char * suffix);

void addMimeTypeToInputPlugin(InputPlugin * inPlugin, char * suffix);

void freeInputPlugin(InputPlugin * inputPlugin);

/* interface for using plugins */

InputPlugin * getInputPluginFromSuffix(char * suffix);

InputPlugin * getInputPluginFromMimeTypes(char * mimeType);

InputPlugin * getInputPluginFromName(char * name);

/* this is needed b/c shoutcast and such don't indicate the proper mime type
	and don't have a suffix in the url! */
InputPlugin * getDefaultInputPlugin();

/* this is where we "load" all the "plugins" ;-) */
void initInputPlugins();

/* this is where we "unload" all the "plugins" */
void finishInputPlugins();

void unloadInputPlugin(InputPlugin * inputPlugin);

#endif
