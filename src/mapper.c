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

/*
 * Maps directory and song objects to file system paths.
 */

#include "mapper.h"
#include "directory.h"
#include "song.h"
#include "path.h"

const char *
map_directory_fs(const struct directory *directory, char *buffer)
{
	const char *dirname = directory_get_path(directory);
	if (isRootDirectory(dirname))
	    return musicDir;

	return rmp2amp_r(buffer, utf8_to_fs_charset(buffer, dirname));
}

const char *
map_directory_child_fs(const struct directory *directory, const char *name,
		       char *buffer)
{
	char buffer2[MPD_PATH_MAX];
	const char *parent_fs;

	parent_fs = map_directory_fs(directory, buffer2);
	if (parent_fs == NULL)
		return NULL;

	utf8_to_fs_charset(buffer, name);
	pfx_dir(buffer, name, strlen(name),
		parent_fs, strlen(parent_fs));
	return buffer;
}

const char *
map_song_fs(const struct song *song, char *buffer)
{
	assert(song->parent != NULL);

	return map_directory_child_fs(song->parent, song->url, buffer);
}

const char *
map_fs_to_utf8(const char *path_fs, char *buffer)
{
	size_t music_path_length = strlen(musicDir);

	if (strncmp(path_fs, musicDir, music_path_length) == 0 &&
	    path_fs[music_path_length] == '/')
		/* remove musicDir prefix */
		path_fs += music_path_length;
	else if (path_fs[0] == '/')
		/* not within musicDir */
		return NULL;

	return fs_charset_to_utf8(buffer, path_fs);
}
