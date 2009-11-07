/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_DECODER_LIST_H
#define MPD_DECODER_LIST_H

#include <stdbool.h>

struct decoder_plugin;

extern const struct decoder_plugin *const decoder_plugins[];
extern bool decoder_plugins_enabled[];

/* interface for using plugins */

/**
 * Find the next enabled decoder plugin which supports the specified suffix.
 *
 * @param suffix the file name suffix
 * @param plugin the previous plugin, or NULL to find the first plugin
 * @return a plugin, or NULL if none matches
 */
const struct decoder_plugin *
decoder_plugin_from_suffix(const char *suffix,
			   const struct decoder_plugin *plugin);

const struct decoder_plugin *
decoder_plugin_from_mime_type(const char *mimeType, unsigned int next);

const struct decoder_plugin *
decoder_plugin_from_name(const char *name);

/* this is where we "load" all the "plugins" ;-) */
void decoder_plugin_init_all(void);

/* this is where we "unload" all the "plugins" */
void decoder_plugin_deinit_all(void);

#endif
