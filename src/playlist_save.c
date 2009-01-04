/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
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

#include "playlist_save.h"
#include "playlist.h"
#include "song.h"
#include "mapper.h"
#include "path.h"
#include "ls.h"
#include "database.h"

#include <glib.h>

void
playlist_print_song(FILE *file, const struct song *song)
{
	if (playlist_saveAbsolutePaths && song_in_database(song)) {
		char *path = map_song_fs(song);
		if (path != NULL) {
			fprintf(file, "%s\n", path);
			g_free(path);
		}
	} else {
		char *uri = song_get_uri(song);
		char tmp2[MPD_PATH_MAX];

		utf8_to_fs_charset(tmp2, uri);
		g_free(uri);

		fprintf(file, "%s\n", uri);
	}
}

void
playlist_print_uri(FILE *file, const char *uri)
{
	char tmp[MPD_PATH_MAX];
	char *s;

	if (playlist_saveAbsolutePaths && !uri_has_scheme(uri) &&
	    uri[0] != '/')
		s = map_uri_fs(uri);
	else
		s = g_strdup(utf8_to_fs_charset(tmp, uri));

	if (s != NULL) {
		fprintf(file, "%s\n", s);
		g_free(s);
	}
}
