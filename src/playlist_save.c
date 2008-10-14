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

void
playlist_print_song(FILE *file, const struct song *song)
{
	char tmp1[MPD_PATH_MAX], tmp2[MPD_PATH_MAX];

	if (playlist_saveAbsolutePaths && song_is_file(song)) {
		const char *path = map_song_fs(song, tmp1);
		if (path != NULL)
			fprintf(file, "%s\n", path);
	} else {
		song_get_url(song, tmp1);
		utf8_to_fs_charset(tmp2, tmp1);
		fprintf(file, "%s\n", tmp2);
	}
}

void
playlist_print_uri(FILE *file, const char *uri)
{
	char tmp[MPD_PATH_MAX];
	const char *s;

	if (playlist_saveAbsolutePaths && !isValidRemoteUtf8Url(s))
		s = map_directory_child_fs(db_get_root(), uri, tmp);
	else
		s = utf8_to_fs_charset(tmp, uri);

	fprintf(file, "%s\n", s);
}
