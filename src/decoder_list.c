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

#include "decoder_list.h"
#include "decoder_api.h"

extern struct decoder_plugin mp3Plugin;
extern struct decoder_plugin oggvorbisPlugin;
extern struct decoder_plugin flacPlugin;
extern struct decoder_plugin oggflacPlugin;
extern struct decoder_plugin audiofilePlugin;
extern struct decoder_plugin mp4Plugin;
extern struct decoder_plugin aacPlugin;
extern struct decoder_plugin mpcPlugin;
extern struct decoder_plugin wavpackPlugin;
extern struct decoder_plugin modPlugin;

static List *inputPlugin_list;

void decoder_plugin_load(struct decoder_plugin * inputPlugin)
{
	if (!inputPlugin)
		return;
	if (!inputPlugin->name)
		return;

	if (inputPlugin->init_func && inputPlugin->init_func() < 0)
		return;

	insertInList(inputPlugin_list, inputPlugin->name, (void *)inputPlugin);
}

void decoder_plugin_unload(struct decoder_plugin * inputPlugin)
{
	if (inputPlugin->finish_func)
		inputPlugin->finish_func();
	deleteFromList(inputPlugin_list, inputPlugin->name);
}

static int stringFoundInStringArray(const char *const*array, const char *suffix)
{
	while (array && *array) {
		if (strcasecmp(*array, suffix) == 0)
			return 1;
		array++;
	}

	return 0;
}

struct decoder_plugin *decoder_plugin_from_suffix(const char *suffix,
						  unsigned int next)
{
	static ListNode *pos;
	ListNode *node;
	struct decoder_plugin *plugin;

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

struct decoder_plugin *decoder_plugin_from_mime_type(const char *mimeType,
						     unsigned int next)
{
	static ListNode *pos;
	ListNode *node;
	struct decoder_plugin *plugin;

	if (mimeType == NULL)
		return NULL;

	node = (next && pos) ? pos : inputPlugin_list->firstNode;

	while (node != NULL) {
		plugin = node->data;
		if (stringFoundInStringArray(plugin->mime_types, mimeType)) {
			pos = node->nextNode;
			return plugin;
		}
		node = node->nextNode;
	}

	return NULL;
}

struct decoder_plugin *decoder_plugin_from_name(const char *name)
{
	void *plugin = NULL;

	findInList(inputPlugin_list, name, &plugin);

	return (struct decoder_plugin *) plugin;
}

void decoder_plugin_print_all_suffixes(FILE * fp)
{
	ListNode *node = inputPlugin_list->firstNode;
	struct decoder_plugin *plugin;
	const char *const*suffixes;

	while (node) {
		plugin = (struct decoder_plugin *) node->data;
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

void decoder_plugin_init_all(void)
{
	inputPlugin_list = makeList(NULL, 1);

	/* load plugins here */
	decoder_plugin_load(&mp3Plugin);
	decoder_plugin_load(&oggvorbisPlugin);
	decoder_plugin_load(&oggflacPlugin);
	decoder_plugin_load(&flacPlugin);
	decoder_plugin_load(&audiofilePlugin);
	decoder_plugin_load(&mp4Plugin);
	decoder_plugin_load(&aacPlugin);
	decoder_plugin_load(&mpcPlugin);
	decoder_plugin_load(&wavpackPlugin);
	decoder_plugin_load(&modPlugin);
}

void decoder_plugin_deinit_all(void)
{
	freeList(inputPlugin_list);
}
