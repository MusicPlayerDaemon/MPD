/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu)
 * This project's homepage is: http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "inputPlugin.h"

#include "list.h"
#include "myfprintf.h"

#include <stdlib.h>
#include <string.h>

static List * inputPlugin_list = NULL;

void loadInputPlugin(InputPlugin * inputPlugin) {
	if(!inputPlugin) return;
	if(!inputPlugin->name) return;

	if(inputPlugin->initFunc && inputPlugin->initFunc() < 0) return;

	insertInList(inputPlugin_list, inputPlugin->name, (void *)inputPlugin);
}

void unloadInputPlugin(InputPlugin * inputPlugin) {
	if(inputPlugin->finishFunc) inputPlugin->finishFunc();
	deleteFromList(inputPlugin_list, inputPlugin->name);
}

static int stringFoundInStringArray(char ** array, char * suffix) {
	while(array && *array) {
		if(strcasecmp(*array, suffix) == 0) return 1;
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

	if(mimeType == NULL) return NULL;

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

void printAllInputPluginSuffixes(FILE * fp) {
	ListNode * node = inputPlugin_list->firstNode;
	InputPlugin * plugin;
	char ** suffixes;

	while(node) {
		plugin = (InputPlugin *)node->data;
		suffixes = plugin->suffixes;
		while(suffixes && *suffixes) {
			myfprintf(fp, "%s ", *suffixes);
			suffixes++;
		}
		node = node->nextNode;
	}
	myfprintf(fp, "\n");
}

extern InputPlugin mp3Plugin;
extern InputPlugin oggPlugin;
extern InputPlugin flacPlugin;
extern InputPlugin audiofilePlugin;
extern InputPlugin mp4Plugin;
extern InputPlugin aacPlugin;
extern InputPlugin modPlugin;

void initInputPlugins() {
	inputPlugin_list = makeList(NULL);

	/* load plugins here */
	loadInputPlugin(&mp3Plugin);
	loadInputPlugin(&oggPlugin);
	loadInputPlugin(&flacPlugin);
	loadInputPlugin(&audiofilePlugin);
	loadInputPlugin(&mp4Plugin);
	loadInputPlugin(&modPlugin);
}

void finishInputPlugins() {
	freeList(inputPlugin_list);
}
