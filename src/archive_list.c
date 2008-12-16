


/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Viliam Mateicka <viliam.mateicka@gmail.com>
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

#include "archive_list.h"
#include "archive_api.h"
#include "utils.h"
#include "../config.h"

#include <string.h>
#include <glib.h>

extern const struct archive_plugin bz2_plugin;
extern const struct archive_plugin zip_plugin;

static const struct archive_plugin *const archive_plugins[] = {
#ifdef HAVE_BZ2
	&bz2_plugin,
#endif
#ifdef HAVE_ZIP
	&zip_plugin,
#endif
	NULL
};

enum {
	num_archive_plugins = G_N_ELEMENTS(archive_plugins)-1,
};

/** which plugins have been initialized successfully? */
static bool archive_plugins_enabled[num_archive_plugins+1];

const struct archive_plugin *
archive_plugin_from_suffix(const char *suffix)
{
	unsigned i;

	if (suffix == NULL)
		return NULL;

	for (i=0; i < num_archive_plugins; ++i) {
		const struct archive_plugin *plugin = archive_plugins[i];
		if (archive_plugins_enabled[i] &&
		    stringFoundInStringArray(plugin->suffixes, suffix)) {
			++i;
			return plugin;
		}
	}
	return NULL;
}

const struct archive_plugin *
archive_plugin_from_name(const char *name)
{
	for (unsigned i = 0; i < num_archive_plugins; ++i) {
		const struct archive_plugin *plugin = archive_plugins[i];
		if (archive_plugins_enabled[i] &&
		    strcmp(plugin->name, name) == 0)
			return plugin;
	}
	return NULL;
}

void archive_plugin_print_all_suffixes(FILE * fp)
{
	const char *const*suffixes;

	for (unsigned i = 0; i < num_archive_plugins; ++i) {
		const struct archive_plugin *plugin = archive_plugins[i];
		if (!archive_plugins_enabled[i])
			continue;

		suffixes = plugin->suffixes;
		while (suffixes && *suffixes) {
			fprintf(fp, "%s ", *suffixes);
			suffixes++;
		}
	}
	fprintf(fp, "\n");
	fflush(fp);
}

void archive_plugin_init_all(void)
{
	for (unsigned i = 0; i < num_archive_plugins; ++i) {
		const struct archive_plugin *plugin = archive_plugins[i];
		if (plugin->init == NULL || archive_plugins[i]->init())
			archive_plugins_enabled[i] = true;
	}
}

void archive_plugin_deinit_all(void)
{
	for (unsigned i = 0; i < num_archive_plugins; ++i) {
		const struct archive_plugin *plugin = archive_plugins[i];
		if (archive_plugins_enabled[i] && plugin->finish != NULL)
			archive_plugins[i]->finish();
	}
}

