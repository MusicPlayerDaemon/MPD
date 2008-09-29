/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#include "song.h"
#include "ls.h"
#include "directory.h"
#include "utils.h"
#include "log.h"
#include "path.h"
#include "playlist.h"
#include "decoder_list.h"
#include "decoder_api.h"

#include "os_compat.h"

Song *newNullSong(void)
{
	Song *song = xmalloc(sizeof(Song));

	song->tag = NULL;
	song->url = NULL;
	song->type = SONG_TYPE_FILE;
	song->parentDir = NULL;

	return song;
}

Song *newSong(const char *url, enum song_type type, Directory * parentDir)
{
	Song *song;

	if (strchr(url, '\n')) {
		DEBUG("newSong: '%s' is not a valid uri\n", url);
		return NULL;
	}

	song = newNullSong();

	song->url = xstrdup(url);
	song->type = type;
	song->parentDir = parentDir;

	assert(type == SONG_TYPE_URL || parentDir);

	if (song->type == SONG_TYPE_FILE) {
		struct decoder_plugin *plugin;
		unsigned int next = 0;
		char path_max_tmp[MPD_PATH_MAX];
		char *abs_path = rmp2amp_r(path_max_tmp,
		                           get_song_url(path_max_tmp, song));

		while (!song->tag && (plugin = isMusic(abs_path,
						       &(song->mtime),
						       next++))) {
			song->tag = plugin->tag_dup(abs_path);
		}
		if (!song->tag || song->tag->time < 0) {
			freeSong(song);
			song = NULL;
		}
	}

	return song;
}

void freeSong(Song * song)
{
	deleteASongFromPlaylist(song);
	freeJustSong(song);
}

void freeJustSong(Song * song)
{
	free(song->url);
	if (song->tag)
		tag_free(song->tag);
	free(song);
}

int updateSongInfo(Song * song)
{
	if (song->type == SONG_TYPE_FILE) {
		struct decoder_plugin *plugin;
		unsigned int next = 0;
		char path_max_tmp[MPD_PATH_MAX];
		char abs_path[MPD_PATH_MAX];

		utf8_to_fs_charset(abs_path, get_song_url(path_max_tmp, song));
		rmp2amp_r(abs_path, abs_path);

		if (song->tag)
			tag_free(song->tag);

		song->tag = NULL;

		while (!song->tag && (plugin = isMusic(abs_path,
						       &(song->mtime),
						       next++))) {
			song->tag = plugin->tag_dup(abs_path);
		}
		if (!song->tag || song->tag->time < 0)
			return -1;
	}

	return 0;
}

char *get_song_url(char *path_max_tmp, Song *song)
{
	if (!song)
		return NULL;

	assert(song->url != NULL);

	if (!song->parentDir || !song->parentDir->path)
		strcpy(path_max_tmp, song->url);
	else
		pfx_dir(path_max_tmp, song->url, strlen(song->url),
			getDirectoryPath(song->parentDir),
			strlen(getDirectoryPath(song->parentDir)));
	return path_max_tmp;
}
