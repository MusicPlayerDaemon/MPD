/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_ARCHIVE_LIST_H
#define MPD_ARCHIVE_LIST_H

struct archive_plugin;

extern const struct archive_plugin *const archive_plugins[];

#define archive_plugins_for_each(plugin) \
	for (const struct archive_plugin *plugin, \
		*const*archive_plugin_iterator = &archive_plugins[0]; \
		(plugin = *archive_plugin_iterator) != NULL; \
		++archive_plugin_iterator)

/* interface for using plugins */

const struct archive_plugin *
archive_plugin_from_suffix(const char *suffix);

const struct archive_plugin *
archive_plugin_from_name(const char *name);

/* this is where we "load" all the "plugins" ;-) */
void archive_plugin_init_all(void);

/* this is where we "unload" all the "plugins" */
void archive_plugin_deinit_all(void);

#endif
