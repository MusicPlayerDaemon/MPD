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
#include "playlist_database.h"
#include "playlist_vector.h"
#include "text_file.h"
#include "string_util.h"

#include <string.h>
#include <stdlib.h>

static GQuark
playlist_database_quark(void)
{
	return g_quark_from_static_string("playlist_database");
}

void
playlist_vector_save(FILE *fp, const struct list_head *pv)
{
	struct playlist_metadata *pm;
	playlist_vector_for_each(pm, pv)
		fprintf(fp, PLAYLIST_META_BEGIN "%s\n"
			"mtime: %li\n"
			"playlist_end\n",
			pm->name, (long)pm->mtime);
}

bool
playlist_metadata_load(FILE *fp, struct list_head *pv, const char *name,
		       GString *buffer, GError **error_r)
{
	struct playlist_metadata pm = {
		.mtime = 0,
	};
	char *line, *colon;
	const char *value;

	while ((line = read_text_line(fp, buffer)) != NULL &&
	       strcmp(line, "playlist_end") != 0) {
		colon = strchr(line, ':');
		if (colon == NULL || colon == line) {
			g_set_error(error_r, playlist_database_quark(), 0,
				    "unknown line in db: %s", line);
			return false;
		}

		*colon++ = 0;
		value = strchug_fast_c(colon);

		if (strcmp(line, "mtime") == 0)
			pm.mtime = strtol(value, NULL, 10);
		else {
			g_set_error(error_r, playlist_database_quark(), 0,
				    "unknown line in db: %s", line);
			return false;
		}
	}

	playlist_vector_update_or_add(pv, name, pm.mtime);
	return true;
}
