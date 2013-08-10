/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "PlaylistDatabase.hxx"
#include "PlaylistVector.hxx"
#include "TextFile.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <string.h>
#include <stdlib.h>

static constexpr Domain playlist_database_domain("playlist_database");

void
playlist_vector_save(FILE *fp, const PlaylistVector &pv)
{
	for (const PlaylistInfo &pi : pv)
		fprintf(fp, PLAYLIST_META_BEGIN "%s\n"
			"mtime: %li\n"
			"playlist_end\n",
			pi.name.c_str(), (long)pi.mtime);
}

bool
playlist_metadata_load(TextFile &file, PlaylistVector &pv, const char *name,
		       Error &error)
{
	PlaylistInfo pm(name, 0);

	char *line, *colon;
	const char *value;

	while ((line = file.ReadLine()) != NULL &&
	       strcmp(line, "playlist_end") != 0) {
		colon = strchr(line, ':');
		if (colon == NULL || colon == line) {
			error.Format(playlist_database_domain,
				     "unknown line in db: %s", line);
			return false;
		}

		*colon++ = 0;
		value = strchug_fast_c(colon);

		if (strcmp(line, "mtime") == 0)
			pm.mtime = strtol(value, NULL, 10);
		else {
			error.Format(playlist_database_domain,
				     "unknown line in db: %s", line);
			return false;
		}
	}

	pv.UpdateOrInsert(std::move(pm));
	return true;
}
