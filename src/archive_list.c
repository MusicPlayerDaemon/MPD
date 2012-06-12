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

#include "config.h"
#include "archive_list.h"
#include "archive_plugin.h"
#include "string_util.h"
#include "archive/bz2_archive_plugin.h"
#include "archive/iso9660_archive_plugin.h"
#include "archive/zzip_archive_plugin.h"

#include <string.h>
#include <glib.h>

const struct archive_plugin *const archive_plugins[] = {
#ifdef HAVE_BZ2
	&bz2_archive_plugin,
#endif
#ifdef HAVE_ZZIP
	&zzip_archive_plugin,
#endif
#ifdef HAVE_ISO9660
	&iso9660_archive_plugin,
#endif
	NULL
};

/** which plugins have been initialized successfully? */
static bool archive_plugins_enabled[G_N_ELEMENTS(archive_plugins) - 1];

#define archive_plugins_for_each_enabled(plugin) \
	archive_plugins_for_each(plugin) \
		if (archive_plugins_enabled[archive_plugin_iterator - archive_plugins])

const struct archive_plugin *
archive_plugin_from_suffix(const char *suffix)
{
	if (suffix == NULL)
		return NULL;

	archive_plugins_for_each_enabled(plugin)
		if (plugin->suffixes != NULL &&
		    string_array_contains(plugin->suffixes, suffix))
			return plugin;

	return NULL;
}

const struct archive_plugin *
archive_plugin_from_name(const char *name)
{
	archive_plugins_for_each_enabled(plugin)
		if (strcmp(plugin->name, name) == 0)
			return plugin;

	return NULL;
}

void archive_plugin_init_all(void)
{
	for (unsigned i = 0; archive_plugins[i] != NULL; ++i) {
		const struct archive_plugin *plugin = archive_plugins[i];
		if (plugin->init == NULL || archive_plugins[i]->init())
			archive_plugins_enabled[i] = true;
	}
}

void archive_plugin_deinit_all(void)
{
	archive_plugins_for_each_enabled(plugin)
		if (plugin->finish != NULL)
			plugin->finish();
}

