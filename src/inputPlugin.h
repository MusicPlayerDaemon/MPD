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

#ifndef INPUT_PLUGIN_H
#define INPUT_PLUGIN_H

#include "os_compat.h"

struct decoder_plugin;

/* individual functions to load/unload plugins */
void loadInputPlugin(struct decoder_plugin * inputPlugin);
void unloadInputPlugin(struct decoder_plugin * inputPlugin);

/* interface for using plugins */

struct decoder_plugin *getInputPluginFromSuffix(const char *suffix, unsigned int next);

struct decoder_plugin *getInputPluginFromMimeType(const char *mimeType, unsigned int next);

struct decoder_plugin *getInputPluginFromName(const char *name);

void printAllInputPluginSuffixes(FILE * fp);

/* this is where we "load" all the "plugins" ;-) */
void initInputPlugins(void);

/* this is where we "unload" all the "plugins" */
void finishInputPlugins(void);

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

#endif
