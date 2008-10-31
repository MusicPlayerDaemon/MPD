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
#include "conf.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>

static char *music_dir;
static size_t music_dir_length;

static char *playlist_dir;
static size_t playlist_dir_length;

void mapper_init(void)
{
	ConfigParam *music_dir_param = parseConfigFilePath(CONF_MUSIC_DIR, 1);
	ConfigParam *playlist_dir_param = parseConfigFilePath(CONF_PLAYLIST_DIR, 1);
	int ret;
	struct stat st;

	music_dir = g_strdup(music_dir_param->value);
	music_dir_length = strlen(music_dir);

	ret = stat(music_dir, &st);
	if (ret < 0)
		g_warning("failed to stat music directory \"%s\" (config line %i): %s\n",
			  music_dir_param->value, music_dir_param->line,
			  strerror(errno));
	else if (!S_ISDIR(st.st_mode))
		g_warning("music directory is not a directory: \"%s\" (config line %i)\n",
			  music_dir_param->value, music_dir_param->line);

	playlist_dir = g_strdup(playlist_dir_param->value);
	playlist_dir_length = strlen(playlist_dir);

	ret = stat(playlist_dir, &st);
	if (ret < 0)
		g_warning("failed to stat playlist directory \"%s\" (config line %i): %s\n",
			  playlist_dir_param->value, playlist_dir_param->line,
			  strerror(errno));
	else if (!S_ISDIR(st.st_mode))
		g_warning("playlist directory is not a directory: \"%s\" (config line %i)\n",
			  playlist_dir_param->value, playlist_dir_param->line);
}

void mapper_finish(void)
{
	g_free(music_dir);
	g_free(playlist_dir);
}

static char *
rmp2amp_r(char *dst, const char *rel_path)
{
	pfx_dir(dst, rel_path, strlen(rel_path),
		(const char *)music_dir, music_dir_length);
	return dst;
}

const char *
map_directory_fs(const struct directory *directory, char *buffer)
{
	const char *dirname = directory_get_path(directory);
	if (isRootDirectory(dirname))
	    return music_dir;

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
	assert(song_is_file(song));

	if (song_in_database(song))
		return map_directory_child_fs(song->parent, song->url, buffer);
	else
		return utf8_to_fs_charset(buffer, song->url);
}

const char *
map_fs_to_utf8(const char *path_fs, char *buffer)
{
	if (strncmp(path_fs, music_dir, music_dir_length) == 0 &&
	    path_fs[music_dir_length] == '/')
		/* remove musicDir prefix */
		path_fs += music_dir_length + 1;
	else if (path_fs[0] == '/')
		/* not within musicDir */
		return NULL;

	return fs_charset_to_utf8(buffer, path_fs);
}

static char *rpp2app_r(char *dst, const char *rel_path)
{
	pfx_dir(dst, rel_path, strlen(rel_path),
		playlist_dir, playlist_dir_length);
	return dst;
}

const char *
map_spl_path(void)
{
	return playlist_dir;
}

const char *
map_spl_utf8_to_fs(const char *name, char *buffer)
{
	rpp2app_r(buffer, utf8_to_fs_charset(buffer, name));
	g_strlcat(buffer, "." PLAYLIST_FILE_SUFFIX, MPD_PATH_MAX);
	return buffer;
}
