/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

static List *inputPlugin_list;

void loadInputPlugin(InputPlugin * inputPlugin)
{
	if (!inputPlugin)
		return;
	if (!inputPlugin->name)
		return;

	if (inputPlugin->initFunc && inputPlugin->initFunc() < 0)
		return;

	insertInList(inputPlugin_list, inputPlugin->name, (void *)inputPlugin);
}

void unloadInputPlugin(InputPlugin * inputPlugin)
{
	if (inputPlugin->finishFunc)
		inputPlugin->finishFunc();
	deleteFromList(inputPlugin_list, inputPlugin->name);
}

static int stringFoundInStringArray(char **array, char *suffix)
{
	while (array && *array) {
		if (strcasecmp(*array, suffix) == 0)
			return 1;
		array++;
	}

	return 0;
}

InputPlugin *getInputPluginFromSuffix(char *suffix, unsigned int next)
{
	static ListNode *pos;
	ListNode *node;
	InputPlugin *plugin;

	if (suffix == NULL)
		return NULL;

	if (next) {
		if (pos)
			node = pos;
		else
			return NULL;
	} else
		node = inputPlugin_list->firstNode;

	while (node != NULL) {
		plugin = node->data;
		if (stringFoundInStringArray(plugin->suffixes, suffix)) {
			pos = node->nextNode;
			return plugin;
		}
		node = node->nextNode;
	}

	return NULL;
}

InputPlugin *getInputPluginFromMimeType(char *mimeType, unsigned int next)
{
	static ListNode *pos;
	ListNode *node;
	InputPlugin *plugin;

	if (mimeType == NULL)
		return NULL;

	node = (next && pos) ? pos : inputPlugin_list->firstNode;

	while (node != NULL) {
		plugin = node->data;
		if (stringFoundInStringArray(plugin->mimeTypes, mimeType)) {
			pos = node->nextNode;
			return plugin;
		}
		node = node->nextNode;
	}

	return NULL;
}

InputPlugin *getInputPluginFromName(char *name)
{
	void *plugin = NULL;

	findInList(inputPlugin_list, name, &plugin);

	return (InputPlugin *) plugin;
}

void printAllInputPluginSuffixes(FILE * fp)
{
	ListNode *node = inputPlugin_list->firstNode;
	InputPlugin *plugin;
	char **suffixes;

	while (node) {
		plugin = (InputPlugin *) node->data;
		suffixes = plugin->suffixes;
		while (suffixes && *suffixes) {
			fprintf(fp, "%s ", *suffixes);
			suffixes++;
		}
		node = node->nextNode;
	}
	fprintf(fp, "\n");
	fflush(fp);
}

void initInputPlugins(void)
{
	inputPlugin_list = makeList(NULL, 1);

	/* load plugins here */
	loadInputPlugin(&mp3Plugin);
	loadInputPlugin(&oggvorbisPlugin);
	loadInputPlugin(&oggflacPlugin);
	loadInputPlugin(&flacPlugin);
	loadInputPlugin(&audiofilePlugin);
	loadInputPlugin(&mp4Plugin);
	loadInputPlugin(&mpcPlugin);
	loadInputPlugin(&modPlugin);
}

void finishInputPlugins(void)
{
	freeList(inputPlugin_list);
}
