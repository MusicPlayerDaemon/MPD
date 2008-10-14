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
#include "path.h"
#include "ls.h"

void
playlist_print_song(FILE *file, const struct song *song)
{
	char tmp1[MPD_PATH_MAX], tmp2[MPD_PATH_MAX];

	song_get_url(song, tmp1);
	utf8_to_fs_charset(tmp2, tmp1);

	if (playlist_saveAbsolutePaths && song_is_file(song))
		fprintf(file, "%s\n", rmp2amp_r(tmp2, tmp2));
	else
		fprintf(file, "%s\n", tmp2);
}

void
playlist_print_uri(FILE *file, const char *uri)
{
	char tmp[MPD_PATH_MAX];
	const char *s;

	s = utf8_to_fs_charset(tmp, uri);
	if (playlist_saveAbsolutePaths && !isValidRemoteUtf8Url(s))
		s = rmp2amp_r(tmp, s);
	fprintf(file, "%s\n", s);
}
